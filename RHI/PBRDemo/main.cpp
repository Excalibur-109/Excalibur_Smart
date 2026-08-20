#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>

#include "Math.hpp"
#include "PBRDemoConfig.hpp"
#include "RHI.hpp"
#include "RHI/RenderPipelineDemo/ForwardRenderPipeline.hpp"
#include "RHI/Renderer/RHIDrawPreparation.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include <stb_image.h>

#if defined(PBRDEMO_ENABLE_RENDERDOC_CAPTURE)
#include <renderdoc_app.h>
#endif

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

// 同一套场景引导代码可编译为 PBRDemo 或 RenderPipelineDemo；后者唯一改变
// 是可执行文件/窗口的身份，渲染路径仍通过下面的通用 ForwardRenderPipeline。
#if defined(RHI_RENDER_PIPELINE_DEMO)
constexpr const wchar_t* DEMO_WINDOW_TITLE = L"RHI Generic Forward Pipeline Demo - ";
constexpr const char* DEMO_APPLICATION_NAME = "RHI Generic Forward Pipeline Demo";
constexpr const char* DEMO_CAPTURE_TITLE = "RenderPipelineDemo textured material frame";
constexpr const char* DEMO_ERROR_TITLE = "RHI Render Pipeline Demo";
#else
constexpr const wchar_t* DEMO_WINDOW_TITLE = L"RHI RenderGraph PBR Demo - ";
constexpr const char* DEMO_APPLICATION_NAME = "RHI RenderGraph PBR Demo";
constexpr const char* DEMO_CAPTURE_TITLE = "PBRDemo textured material frame";
constexpr const char* DEMO_ERROR_TITLE = "RHI PBR Demo";
#endif

// 本文件是一个“从窗口到提交”的完整学习样例：
//   Win32 HWND -> RHIDevice -> 纹理/缓冲/管线 -> RenderGraph pass -> Present。
// PBRDemo 只调用 RHI 的公共描述符，后端差异集中在 shader 文件路径和窗口创建参数；
// 这使同一份场景代码可以在 Vulkan、D3D11、D3D12 间切换，而无需在这里写原生命令。
// UI 也以同样原则作为上层模块接入，UI pass 在 OpaquePBR 之后使用 Load 保留场景颜色。

constexpr rhi::u32 WINDOW_WIDTH = 1280;     ///< 窗口模式下的默认客户区宽度。
constexpr rhi::u32 WINDOW_HEIGHT = 800;     ///< 窗口模式下的默认客户区高度。
constexpr rhi::u32 FRAMES_IN_FLIGHT = 2;    ///< CPU 可提前准备的最大帧数，也是二进制信号的轮转数量。
constexpr rhi::u32 SHADOW_MAP_SIZE = 2048;  ///< 阴影图边长；精度与显存、清理和采样成本之间的折中。
constexpr float PI = 3.14159265359F;        ///< 球面参数化使用的单精度圆周率。

/// PBR 与 Shadow Pipeline 共用的交错顶点格式。
struct Vertex {
    float3 position{};  ///< 模型空间位置。
    float3 normal{};    ///< 模型空间单位法线。
    float2 uv{};        ///< 纹理坐标；当前材质仍保留它以演示完整顶点布局。
    float4 tangent{};   ///< xyz 为切线，w 为 bitangent handedness，供 normal/height map 建立 TBN。
};

/// 与 GLSL std140/HLSL cbuffer 对应的每物体常量数据，16 字节对齐后可跨后端上传。
struct alignas(16) UniformBufferObject {
    // 所有成员按 16 字节边界组织，保证 C++、GLSL std140 与 HLSL cbuffer 的偏移一致。
    // 每帧只更新两个 UBO（球体和地面），天空盒复用球体的相机矩阵与环境贴图。
    // 主相机和物体变换。Shader 中按 projection * view * model * position 使用。
    float4x4 model{1.0F};       ///< 模型空间到世界空间。
    float4x4 view{1.0F};        ///< 世界空间到主相机观察空间。
    float4x4 projection{1.0F};  ///< 主相机观察空间到裁剪空间。

    // lightDirection.xyz 表示光线传播方向，因此 Shader 取反得到“表面指向光源”的 L。
    // vec4 可让 C++、GLSL std140 和 HLSL cbuffer 都自然满足 16 字节对齐。
    float4 lightDirection{};      ///< xyz 为世界空间光线传播方向，w 未使用。
    float4 lightColor{};          ///< rgb 为线性空间光强，a 未使用。
    float4 cameraPosition{};      ///< xyz 为世界空间相机位置，w 固定为 1。
    float4 baseColor{};           ///< rgb 为 albedo 贴图乘数，a 为 metallic 贴图乘数。
    float4 materialParameters{};  ///< x 为 roughness 贴图乘数，y 为 AO，z 为 height 偏移，w 为天空盒 Y 轴旋转角（弧度）。

    // lightViewProjection 是生成/查询 Shadow Map 的共同坐标系；shadowParameters 的
    // xy 是单个 texel 的 UV 尺寸，z 是最小 bias，w 是随法线斜率增长的 bias。
    float4x4 lightViewProjection{1.0F};  ///< 世界空间到光源裁剪空间。
    float4 shadowParameters{};           ///< PCF texel 步长与深度比较偏移。
};

/// Math 的 Matrix 是行主序；两个 shader 均按 column_major 接收，因此上传前转置。
[[nodiscard]] float4x4 ToShaderMatrix(const float4x4& matrix) noexcept {
    return Transpose(matrix);
}

/// CPU 侧临时网格，创建 GPU buffer 后即可释放。
struct Mesh {
    std::vector<Vertex> vertices;   ///< 交错顶点数组。
    std::vector<rhi::u32> indices;  ///< 32 位三角形索引数组。
};

/// stb_image 统一转换后的 RGBA8 图像。
struct DecodedImage {
    rhi::u32 width = 0;             ///< 像素宽度。
    rhi::u32 height = 0;            ///< 像素高度。
    std::vector<std::byte> pixels;  ///< 从左上角开始的紧密 RGBA8 像素。
};

/// 通过 stb_image 读取 PNG/JPG 等图像，并强制转换成紧密 RGBA8。
DecodedImage LoadImageRGBA8(std::string_view utf8Path) {
    const std::string path(utf8Path);
    int width = 0;
    int height = 0;
    [[maybe_unused]] int sourceChannels = 0;
    stbi_uc* const decoded = stbi_load(
        path.c_str(),
        &width,
        &height,
        &sourceChannels,
        STBI_rgb_alpha);
    if (decoded == nullptr) {
        const char* const reason = stbi_failure_reason();
        throw std::runtime_error(
            "stb_image could not load '" + path + "': " +
            (reason != nullptr ? reason : "unknown error"));
    }

    if (width <= 0 || height <= 0) {
        stbi_image_free(decoded);
        throw std::runtime_error("Image has invalid dimensions: " + path);
    }
    const std::size_t imageWidth = static_cast<std::size_t>(width);
    const std::size_t imageHeight = static_cast<std::size_t>(height);
    if (imageWidth > std::numeric_limits<std::size_t>::max() / 4U ||
        imageHeight > std::numeric_limits<std::size_t>::max() / (imageWidth * 4U)) {
        stbi_image_free(decoded);
        throw std::runtime_error("Image is too large: " + path);
    }

    DecodedImage result{};
    result.width = static_cast<rhi::u32>(width);
    result.height = static_cast<rhi::u32>(height);
    result.pixels.resize(imageWidth * imageHeight * 4U);
    std::memcpy(result.pixels.data(), decoded, result.pixels.size());
    stbi_image_free(decoded);
    return result;
}

enum class MaterialTextureSlot : std::size_t {
    BaseColor,
    Normal,
    Metallic,
    Roughness,
    Height,
    Count
};

constexpr std::size_t MATERIAL_TEXTURE_COUNT =
    static_cast<std::size_t>(MaterialTextureSlot::Count);

struct MaterialTextureSpec {
    const char* graphName;
    const char* debugName;
    const char* fileName;
    rhi::RHIFormat format;
};

constexpr std::array<MaterialTextureSpec, MATERIAL_TEXTURE_COUNT> MATERIAL_TEXTURE_SPECS = {{
    {"MetalBaseColor", "PBR.Metal.BaseColor", "metal_18_basecolor-2K.png", rhi::RHIFormat::RGBA8_SRGB},
    {"MetalNormal", "PBR.Metal.Normal", "metal_18_normal-2K.png", rhi::RHIFormat::RGBA8_UNorm},
    {"MetalMetallic", "PBR.Metal.Metallic", "metal_18_metallic-2K.png", rhi::RHIFormat::RGBA8_UNorm},
    {"MetalRoughness", "PBR.Metal.Roughness", "metal_18_roughness-2K.png", rhi::RHIFormat::RGBA8_UNorm},
    {"MetalHeight", "PBR.Metal.Height", "metal_18_height-2K.png", rhi::RHIFormat::RGBA8_UNorm},
}};

struct MaterialTexture {
    rhi::RHITexture texture{};
    rhi::RHITextureView view{};
    rhi::u32 width = 0;
    rhi::u32 height = 0;
    std::vector<std::byte> uploadData;
};

/// 从 WinMain 命令行读取的 Demo 启动配置。
struct DemoOptions {
    rhi::RHIGraphicsAPI api = rhi::RHIGraphicsAPI::Vulkan;  ///< `--api=` 选择的 RHI 后端。
    rhi::u64 maxFrames = 0;                                 ///< `--frames=` 上限；0 表示持续运行。
    bool renderDocCapture = false;                           ///< `--renderdoc-capture` 时主动抓取第一张完整帧。
};

/// 解析 `--api=vulkan|d3d11|d3d12`、可选的 `--frames=N` 与 `--renderdoc-capture`。
DemoOptions ParseOptions(std::string_view commandLine) {
    DemoOptions options{};
    std::istringstream arguments{std::string(commandLine)};
    for (std::string argument; arguments >> argument;) {
        constexpr std::string_view apiPrefix = "--api=";
        constexpr std::string_view framesPrefix = "--frames=";
        if (argument.starts_with(apiPrefix)) {
            const std::string_view value(
                argument.data() + apiPrefix.size(),
                argument.size() - apiPrefix.size());
            if (value == "vulkan") {
                options.api = rhi::RHIGraphicsAPI::Vulkan;
            } else if (value == "d3d11") {
                options.api = rhi::RHIGraphicsAPI::D3D11;
            } else if (value == "d3d12") {
                options.api = rhi::RHIGraphicsAPI::D3D12;
            } else {
                throw std::runtime_error("Unknown --api value: " + std::string(value));
            }
        } else if (argument.starts_with(framesPrefix)) {
            const std::string_view value(
                argument.data() + framesPrefix.size(),
                argument.size() - framesPrefix.size());
            const auto [end, error] = std::from_chars(
                value.data(),
                value.data() + value.size(),
                options.maxFrames);
            if (error != std::errc{} || end != value.data() + value.size()) {
                throw std::runtime_error("Invalid --frames value: " + std::string(value));
            }
        } else if (argument == "--renderdoc-capture") {
            options.renderDocCapture = true;
        }
    }
    return options;
}

/// 返回适合显示在 Win32 标题栏中的后端名称。
const wchar_t* ApiDisplayName(rhi::RHIGraphicsAPI api) noexcept {
    switch (api) {
    case rhi::RHIGraphicsAPI::Vulkan: return L"Vulkan";
    case rhi::RHIGraphicsAPI::D3D11:  return L"Direct3D 11";
    case rhi::RHIGraphicsAPI::D3D12:  return L"Direct3D 12";
    default:                           return L"Unknown";
    }
}

/// 创建位于 XZ 平面的方形接收面，正面朝向 +Y。
Mesh MakePlane(float halfSize = 3.25F) {
    Mesh mesh{};
    mesh.vertices = {
        {{-halfSize, 0.0F, -halfSize}, {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F}, {1.0F, 0.0F, 0.0F, -1.0F}},
        {{halfSize, 0.0F, -halfSize}, {0.0F, 1.0F, 0.0F}, {4.0F, 0.0F}, {1.0F, 0.0F, 0.0F, -1.0F}},
        {{halfSize, 0.0F, halfSize}, {0.0F, 1.0F, 0.0F}, {4.0F, 4.0F}, {1.0F, 0.0F, 0.0F, -1.0F}},
        {{-halfSize, 0.0F, halfSize}, {0.0F, 1.0F, 0.0F}, {0.0F, 4.0F}, {1.0F, 0.0F, 0.0F, -1.0F}}};
    // In the demo's left-handed convention, this clockwise index order faces +Y.
    // 一致；否则启用 back-face culling 后整张地面都会被剔除。
    mesh.indices = {0, 2, 1, 0, 3, 2};
    return mesh;
}

/// 通过经纬线参数化生成单位法线平滑的 UV 球体。
Mesh MakeSphere(
    rhi::u32 latitudeCount,
    rhi::u32 longitudeCount,
    float radius = 1.0F) {
    Mesh mesh{};
    mesh.vertices.reserve((latitudeCount + 1) * (longitudeCount + 1));
    mesh.indices.reserve(latitudeCount * longitudeCount * 6);

    for (rhi::u32 latitudeIndex = 0; latitudeIndex <= latitudeCount; ++latitudeIndex) {
        for (rhi::u32 longitudeIndex = 0; longitudeIndex <= longitudeCount; ++longitudeIndex) {
            const float longitudeRatio = static_cast<float>(longitudeIndex) / static_cast<float>(longitudeCount);
            const float latitudeRatio = static_cast<float>(latitudeIndex) / static_cast<float>(latitudeCount);
            const float longitudeAngle = longitudeRatio * 2.0F * PI;
            const float latitudeAngle = latitudeRatio * PI;
            const float sinLatitude = std::sin(latitudeAngle);
            const float3 normal{
                -std::cos(longitudeAngle) * sinLatitude,
                std::cos(latitudeAngle),
                std::sin(longitudeAngle) * sinLatitude};
            const float3 tangent{
                std::sin(longitudeAngle),
                0.0F,
                std::cos(longitudeAngle)};
            mesh.vertices.push_back(
                Vertex{normal * radius, normal, {longitudeRatio, latitudeRatio},
                       {tangent.x, tangent.y, tangent.z, -1.0F}});
        }
    }

    // 相邻两条纬线构成一条纬度带；每次外层循环为该带生成一整圈三角形。
    for (rhi::u32 latitudeIndex = 0; latitudeIndex < latitudeCount; ++latitudeIndex) {
        // 相邻两条经线把纬度带切成一个四边形网格。
        for (rhi::u32 longitudeIndex = 0; longitudeIndex < longitudeCount; ++longitudeIndex) {
            // 每行有 longitudeCount + 1 个顶点，因为 U=0 和 U=1 在接缝处位置相同，
            // 但需要保留两份顶点以分别保存纹理坐标 0 和 1。
            const rhi::u32 upperRow = latitudeIndex * (longitudeCount + 1);
            const rhi::u32 lowerRow = (latitudeIndex + 1) * (longitudeCount + 1);
            const rhi::u32 upperLeft = upperRow + longitudeIndex;
            const rhi::u32 lowerLeft = lowerRow + longitudeIndex;
            const rhi::u32 lowerRight = lowerRow + longitudeIndex + 1;
            const rhi::u32 upperRight = upperRow + longitudeIndex + 1;
            // Clockwise indices keep the UV sphere front faces pointing outward in LH space.
            mesh.indices.insert(
                mesh.indices.end(),
                {upperLeft, lowerLeft, lowerRight, upperLeft, lowerRight, upperRight});
        }
    }
    return mesh;
}

/// 将连续 trivially-copyable 元素复制为 RHI 上传队列使用的字节数组。
template <typename Type>
std::vector<std::byte> ToBytes(const std::vector<Type>& values) {
    std::vector<std::byte> result(values.size() * sizeof(Type));
    if (!result.empty()) {
        std::memcpy(result.data(), values.data(), result.size());
    }
    return result;
}

/// 将单个 trivially-copyable 对象复制为 RHI 上传队列使用的字节数组。
template <typename Type>
std::vector<std::byte> ToBytes(const Type& value) {
    std::vector<std::byte> result(sizeof(Type));
    std::memcpy(result.data(), &value, sizeof(Type));
    return result;
}

/// 串联 Win32 窗口、RHI 资源、RenderGraph 构建和逐帧提交的 PBR 演示程序。
class PBRDemoApp {
public:
    /// 保存启动选项；实际系统资源统一由 Run 按依赖顺序创建。
    explicit PBRDemoApp(DemoOptions options)
        : options_(options) {
    }

    /// 执行完整应用生命周期，并在主循环退出后释放全部 RHI 资源。
    void Run(HINSTANCE instance) {
        // 创建顺序体现资源依赖：窗口先于 surface/device，device 先于静态资源，
        // swapchain 先于依赖颜色格式的 pipeline/UI。Cleanup 使用相反顺序释放。
        CreateWindowHandle(instance);
        CreateDevice(instance);
        ConfigureRenderDocCapture();
        CreateStaticResources();
        CreateSwapchainResources();
        CreateUI();
        MainLoop();
        Cleanup();
    }

private:
    DemoOptions options_{};            ///< 后端和自动退出帧数等启动配置。
    HWND window_ = nullptr;            ///< swapchain 绑定的 Win32 窗口。
    bool running_ = true;              ///< 主消息循环继续运行的标志。
    bool framebufferResized_ = false;  ///< WM_SIZE 或 acquire/submit 失败后请求重建 swapchain。
    bool isFullscreen_ = false;        ///< true 为覆盖主显示器的无边框全屏，false 为 1280x800 窗口。

    // Hold the right mouse button to rotate this camera around its fixed position.
    // PBRDemo uses a left-handed world: +X is right, +Y is up, and +Z is forward.
    float3 cameraPosition_{0.0F, 3.0F, -6.0F};
    float cameraYaw_ = 0.0F;
    float cameraPitch_ = -0.351F;
    bool mouseLookActive_ = false;
    POINT mouseLookRestorePosition_{};
    rhi::ui::InputState uiInput_{};
    std::size_t selectedMaterialTexture_ = 0;
    float sphereRotationDegreesPerSecond_ = 45.0F;
    float skyRotationDegreesPerSecond_ = 2.5F; ///< 天空盒及非地面物体使用的环境旋转速度。

    std::unique_ptr<rhi::RHIDevice> device_;                      ///< 当前选择的 Vulkan/D3D 设备门面。
    // UI follows device_ in member order so it is released before device destruction.
    std::unique_ptr<rhi::ui::Context> ui_;
    rhi::RHISwapchain swapchain_{};                               ///< 窗口呈现链。
    std::vector<rhi::RHITexture> swapchainImages_;                ///< swapchain 暴露的可呈现颜色纹理。
    rhi::RHIFormat swapchainFormat_ = rhi::RHIFormat::Undefined;  ///< Pipeline color attachment 必须匹配的格式。
    rhi::RHIExtent2D swapchainExtent_{};                          ///< 当前可呈现区域的像素尺寸。
    rhi::RHITexture depthTexture_{};                              ///< 与窗口尺寸一致的主相机深度纹理。
    rhi::RHITextureView depthView_{};                             ///< 主深度纹理的 depth aspect 视图。

    rhi::RHIBuffer vertexBuffer_{};         ///< 球体与地面共享的交错顶点缓冲。
    rhi::RHIBuffer indexBuffer_{};          ///< 球体与地面共享的 32 位索引缓冲。
    rhi::RHIBuffer sphereUniformBuffer_{};  ///< 球体每帧 UBO。
    rhi::RHIBuffer planeUniformBuffer_{};   ///< 地面每帧 UBO。

    // Shadow Mapping 的核心资源：
    // 1. ShadowMap Pass 从光源视角把最近深度写入 shadowTexture_；
    // 2. OpaquePBR Pass 通过 shadowView_ + shadowSampler_ 比较当前片元深度；
    // 3. 全过程只发生在 GPU，CPU 不读取阴影图。
    //
    // Texture 是实际显存资源，View 说明如何解释其 depth aspect，Sampler 说明过滤、
    // 越界和深度比较规则。三者职责不同，所以 RHI 将它们拆成三个对象。
    rhi::RHITexture shadowTexture_{};   ///< Shadow Pass 写入、PBR Pass 采样的 D32 深度纹理。
    rhi::RHITextureView shadowView_{};  ///< 仅暴露 shadowTexture_ 的 depth aspect。
    rhi::RHISampler shadowSampler_{};   ///< 执行 LessOrEqual 深度比较和 PCF 基础过滤。

    rhi::RHIBindSetLayout bindSetLayout_{};    ///< 主 PBR 的 UBO、阴影、环境与 metal 贴图资源布局。
    rhi::RHIBindSet sphereBindSet_{};          ///< 球体 UBO 与场景共享纹理绑定。
    rhi::RHIBindSet planeBindSet_{};           ///< 地面 UBO 与场景共享纹理绑定。
    rhi::RHIPipelineLayout pipelineLayout_{};  ///< 主 PBR Pipeline 使用的资源布局集合。
    rhi::RHIPipeline pipeline_{};              ///< 主相机绘制球体和地面的 PBR 图形管线。

    // Shadow Pass 只需要物体 UBO，不需要读取 Shadow Map 自己，因此使用独立 BindSet。
    // 如果直接复用主 PBR BindSet，就可能在同一时刻把 shadowTexture_ 同时绑定为：
    // - DSV/depth attachment：当前 Pass 正在写；
    // - SRV/sampled texture：Shader 准备读。
    // 这是资源读写冲突，D3D11 会强制解绑并报告警告，Vulkan/D3D12 则需要非法状态组合。
    rhi::RHIBindSetLayout shadowBindSetLayout_{};    ///< Shadow Pipeline 仅包含 UBO 的绑定布局。
    rhi::RHIBindSet shadowSphereBindSet_{};          ///< 阴影投射球体使用的 UBO 绑定。
    rhi::RHIPipelineLayout shadowPipelineLayout_{};  ///< depth-only Pipeline 的资源布局集合。
    rhi::RHIPipeline shadowPipeline_{};              ///< 从光源视角写入 D32 的 depth-only 管线。

    // Cube texture 保存环境图，Cube view 负责方向采样，独立 Pipeline 在主深度之后绘制背景。
    rhi::RHITexture skyboxTexture_{};                ///< 六层 RGBA8 sRGB cube-compatible 纹理。
    rhi::RHITextureView skyboxView_{};               ///< 将六个 array layer 解释为 cubemap。
    rhi::RHISampler skyboxSampler_{};                ///< ClampToEdge 线性采样器，避免面边缘重复。
    rhi::RHIBindSetLayout skyboxBindSetLayout_{};    ///< Skybox UBO 与 cubemap 的资源布局。
    rhi::RHIBindSet skyboxBindSet_{};                ///< 相机 UBO 和环境 cubemap 的实际绑定。
    rhi::RHIPipelineLayout skyboxPipelineLayout_{};  ///< Skybox Pipeline 使用的布局集合。
    rhi::RHIPipeline skyboxPipeline_{};              ///< 深度只读、LessOrEqual 的背景绘制管线。

    // metal_18 的颜色、法线、金属度、粗糙度与高度图。颜色图使用 sRGB 格式，其他
    // 数据图保持 UNorm 线性采样；所有贴图共享一个 Repeat sampler。
    std::array<MaterialTexture, MATERIAL_TEXTURE_COUNT> materialTextures_{};
    rhi::RHISampler materialSampler_{};

    std::vector<std::byte> initialVertexData_;                       ///< 首帧上传的合并顶点数据。
    std::vector<std::byte> initialIndexData_;                        ///< 首帧上传的合并索引数据。
    std::array<std::vector<std::byte>, 6> initialSkyboxFaceData_{};  ///< 按 +X/-X/+Y/-Y/+Z/-Z 排列的六面像素。
    bool staticUploadsPending_ = true;                               ///< 静态 buffer/cubemap 仅在首个成功帧前上传。
    rhi::u32 skyboxWidth_ = 0;                                       ///< cubemap 单面的像素宽度。
    rhi::u32 skyboxHeight_ = 0;                                      ///< cubemap 单面的像素高度。
    rhi::u32 sphereVertexCount_ = 0;                                 ///< 计算地面 base-vertex 使用的球体顶点数。
    rhi::u32 sphereIndexCount_ = 0;                                  ///< 球体 draw 的索引数量。
    rhi::u32 planeIndexCount_ = 0;                                   ///< 地面 draw 的索引数量。
    rhi::u64 sphereIndexOffset_ = 0;                                 ///< 合并索引 buffer 中球体的字节偏移。
    rhi::u64 planeIndexOffset_ = 0;                                  ///< 合并索引 buffer 中地面的字节偏移。

    // 每个 frames-in-flight 槽位各自持有 acquire/present 二进制同步信号。
    std::array<rhi::RHIGPUWaitGPUSignal, FRAMES_IN_FLIGHT> imageAvailable_{};  ///< acquire 完成后由提交等待。
    std::array<rhi::RHIGPUWaitGPUSignal, FRAMES_IN_FLIGHT> renderFinished_{};  ///< 图形提交完成后由 present 等待。

    // 只有成功提交后才推进槽位和累计帧号。
    rhi::u32 frameSlot_ = 0;   ///< 当前轮转的 frames-in-flight 槽位。
    rhi::u64 frameIndex_ = 0;  ///< 已成功提交的累计帧号。

#if defined(PBRDEMO_ENABLE_RENDERDOC_CAPTURE)
    RENDERDOC_API_1_6_0* renderDocApi_ = nullptr;
    bool renderDocCapturePending_ = false;
#endif

    /// 球体旋转动画使用的单调时钟原点。
    std::chrono::steady_clock::time_point startTime_ = std::chrono::steady_clock::now();

    /// 将 Win32 消息转发给实例，并把 resize/close 转为主循环状态。
    // Win32 回调只记录输入边沿和 resize 标志，不在消息线程直接调用 RHI。
    // 这样所有 GPU 对象仍由主循环串行访问，避免窗口消息与提交线程交错。
    static LRESULT CALLBACK WindowProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam) {
        if (message == WM_CREATE) {
            const auto* create = reinterpret_cast<const CREATESTRUCT*>(lParam);
            SetWindowLongPtr(
                window,
                GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return 0;
        }
        auto* app = reinterpret_cast<PBRDemoApp*>(
            GetWindowLongPtr(window, GWLP_USERDATA));
        if (app != nullptr) {
            if (message == WM_SIZE) {
                app->framebufferResized_ = true;
                return 0;
            }
            if (message == WM_LBUTTONDOWN) {
                app->uiInput_.mouseX = static_cast<float>(static_cast<short>(LOWORD(lParam)));
                app->uiInput_.mouseY = static_cast<float>(static_cast<short>(HIWORD(lParam)));
                app->uiInput_.leftButtonDown = true;
                app->uiInput_.leftButtonPressed = true;
                SetCapture(window);
                return 0;
            }
            if (message == WM_LBUTTONUP) {
                app->uiInput_.mouseX = static_cast<float>(static_cast<short>(LOWORD(lParam)));
                app->uiInput_.mouseY = static_cast<float>(static_cast<short>(HIWORD(lParam)));
                app->uiInput_.leftButtonDown = false;
                app->uiInput_.leftButtonReleased = true;
                if (!app->mouseLookActive_ && GetCapture() == window) {
                    ReleaseCapture();
                }
                return 0;
            }
            if (message == WM_RBUTTONDOWN) {
                app->BeginMouseLook();
                return 0;
            }
            if (message == WM_MOUSEMOVE) {
                app->uiInput_.mouseX = static_cast<float>(static_cast<short>(LOWORD(lParam)));
                app->uiInput_.mouseY = static_cast<float>(static_cast<short>(HIWORD(lParam)));
                app->UpdateMouseLook();
                return 0;
            }
            if (message == WM_RBUTTONUP || message == WM_CAPTURECHANGED || message == WM_KILLFOCUS) {
                app->uiInput_.leftButtonDown = false;
                app->uiInput_.leftButtonReleased = true;
                app->EndMouseLook();
                return 0;
            }
            if (message == WM_SETCURSOR && app->mouseLookActive_) {
                SetCursor(nullptr);
                return TRUE;
            }
            if (message == WM_CLOSE || message == WM_DESTROY) {
                app->EndMouseLook(false);
                app->running_ = false;
                PostQuitMessage(0);
                return 0;
            }
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    [[nodiscard]] POINT ClientCenterInScreen() const noexcept {
        RECT clientRect{};
        GetClientRect(window_, &clientRect);
        POINT center{
            (clientRect.left + clientRect.right) / 2,
            (clientRect.top + clientRect.bottom) / 2};
        ClientToScreen(window_, &center);
        return center;
    }

    void BeginMouseLook() noexcept {
        // 右键观察使用“锁定光标到客户区中心”的相对位移模式；释放时恢复用户
        // 原来的屏幕位置，避免相机操作把光标留在窗口中央。
        if (mouseLookActive_ || window_ == nullptr) {
            return;
        }

        GetCursorPos(&mouseLookRestorePosition_);
        mouseLookActive_ = true;
        SetCapture(window_);
        SetCursor(nullptr);
        const POINT center = ClientCenterInScreen();
        SetCursorPos(center.x, center.y);
    }

    void UpdateMouseLook() noexcept {
        if (!mouseLookActive_) {
            return;
        }

        POINT cursor{};
        if (GetCursorPos(&cursor) == FALSE) {
            return;
        }
        const POINT center = ClientCenterInScreen();
        const LONG deltaX = cursor.x - center.x;
        const LONG deltaY = cursor.y - center.y;
        if (deltaX == 0 && deltaY == 0) {
            return;
        }

        constexpr float MOUSE_LOOK_SENSITIVITY = 0.0035F;
        constexpr float MAX_CAMERA_PITCH = 1.50F;
        cameraYaw_ += static_cast<float>(deltaX) * MOUSE_LOOK_SENSITIVITY;
        cameraPitch_ = std::clamp(
            cameraPitch_ - static_cast<float>(deltaY) * MOUSE_LOOK_SENSITIVITY,
            -MAX_CAMERA_PITCH,
            MAX_CAMERA_PITCH);
        SetCursorPos(center.x, center.y);
    }

    void EndMouseLook(bool restoreCursor = true) noexcept {
        if (!mouseLookActive_) {
            return;
        }

        mouseLookActive_ = false;
        if (GetCapture() == window_) {
            ReleaseCapture();
        }
        SetCursor(LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)));
        if (restoreCursor) {
            SetCursorPos(mouseLookRestorePosition_.x, mouseLookRestorePosition_.y);
        }
    }

    [[nodiscard]] float3 CameraForward() const noexcept {
        const float cosPitch = std::cos(cameraPitch_);
        return {
            std::sin(cameraYaw_) * cosPitch,
            std::sin(cameraPitch_),
            std::cos(cameraYaw_) * cosPitch};
    }

    /// 注册窗口类并创建窗口模式或无边框全屏窗口。
    void CreateWindowHandle(HINSTANCE instance) {
        // 值初始化会把未显式设置的窗口类成员清零，例如图标、背景画刷和菜单名称。
        WNDCLASSEXW windowClass{};
        // Win32 通过结构体字节数识别 WNDCLASSEXW 的版本，注册前必须填写。
        windowClass.cbSize = sizeof(windowClass);
        // 当窗口宽度或高度变化时，要求系统重绘整个客户区，避免保留旧尺寸的画面。
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        // 指定该窗口类处理消息的回调，WM_CREATE 会在这里保存 PBRDemoApp 指针。
        windowClass.lpfnWndProc = WindowProcedure;
        // 标记窗口类属于当前可执行模块；创建窗口时必须传入同一个模块实例。
        windowClass.hInstance = instance;
        // 从系统资源加载标准箭头光标；资源编号 32512 对应 IDC_ARROW。
        windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        // 设置窗口类的唯一 Unicode 名称，CreateWindowExW 将通过此名称查找该类。
        windowClass.lpszClassName = L"RHIRenderGraphPBRDemo";
        // 注册窗口类并检查返回的 ATOM；返回 0 表示 Win32 注册失败。
        if (RegisterClassExW(&windowClass) == 0) {
            // 没有成功注册窗口类就无法创建 HWND，因此立即终止初始化。
            throw std::runtime_error("RegisterClassEx failed");
        }

        // 先用逻辑渲染尺寸构造客户区矩形：左上角为 (0, 0)，右下角为目标宽高。
        RECT rectangle{0, 0, static_cast<LONG>(WINDOW_WIDTH), static_cast<LONG>(WINDOW_HEIGHT)};
        // 根据普通窗口的边框和标题栏扩张矩形，使最终客户区仍为 WINDOW_WIDTH x WINDOW_HEIGHT；FALSE 表示没有菜单栏。
        AdjustWindowRect(&rectangle, WS_OVERLAPPEDWINDOW, FALSE);

        // 窗口模式默认使用带标题栏、边框以及最小化/最大化按钮的标准顶层窗口样式。
        DWORD windowStyle = WS_OVERLAPPEDWINDOW;
        // 交给系统选择窗口的初始水平位置。
        int windowX = CW_USEDEFAULT;
        // 交给系统选择窗口的初始垂直位置。
        int windowY = CW_USEDEFAULT;
        // AdjustWindowRect 处理后的矩形宽度是包含边框和标题栏的窗口总宽度。
        int windowWidth = rectangle.right - rectangle.left;
        // AdjustWindowRect 处理后的矩形高度是包含边框和标题栏的窗口总高度。
        int windowHeight = rectangle.bottom - rectangle.top;

        // 全屏模式会覆盖上面的普通窗口样式、位置和尺寸，但不会切换显示器分辨率。
        if (isFullscreen_) {
            // 无边框全屏不改变显示器分辨率，只让 WS_POPUP 窗口覆盖整个主显示器。
            // 使用 rcMonitor 而不是 rcWork，确保窗口也覆盖任务栏所在区域。
            // (0, 0) 是用于查询显示器的虚拟桌面坐标；找不到时仍会回退到主显示器。
            const POINT primaryMonitorPoint{0, 0};
            // 获取目标显示器句柄，MONITOR_DEFAULTTOPRIMARY 保证查询失败时返回主显示器。
            const HMONITOR monitor = MonitorFromPoint(
                // 传入上面定义的虚拟桌面坐标。
                primaryMonitorPoint,
                // 指定坐标不属于任何显示器时使用主显示器。
                MONITOR_DEFAULTTOPRIMARY);
            // 值初始化显示器信息，避免未填写的字段包含不确定数据。
            MONITORINFO monitorInfo{};
            // GetMonitorInfoW 依靠 cbSize 判断调用方提供的结构体版本和可写范围。
            monitorInfo.cbSize = sizeof(MONITORINFO);
            // 查询显示器在虚拟桌面中的完整矩形，并检查 Win32 的 BOOL 返回值。
            if (GetMonitorInfoW(monitor, &monitorInfo) == FALSE) {
                // 无法取得显示器范围时不能可靠计算全屏窗口尺寸，因此终止初始化。
                throw std::runtime_error("GetMonitorInfoW failed");
            }

            // WS_POPUP 不带标题栏和边框，适合覆盖显示器的无边框全屏窗口。
            windowStyle = WS_POPUP;
            // 使用显示器矩形左边界作为窗口左上角的 X 坐标，兼容多显示器负坐标。
            windowX = monitorInfo.rcMonitor.left;
            // 使用显示器矩形上边界作为窗口左上角的 Y 坐标，兼容多显示器排列。
            windowY = monitorInfo.rcMonitor.top;
            // Win32 RECT 的 right 是右边界坐标，减去 left 得到显示器实际宽度。
            windowWidth = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
            // Win32 RECT 的 bottom 是下边界坐标，减去 top 得到显示器实际高度。
            windowHeight = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
        }

        // 窗口标题由固定 Demo 名称和当前 RHI 后端名称拼接而成。
        const std::wstring windowTitle =
            // 先构造 std::wstring，确保后续加法执行宽字符串拼接而不是指针运算。
            std::wstring(DEMO_WINDOW_TITLE) +
            // 追加 Vulkan 或 D3D 等当前图形 API 的可读名称。
            ApiDisplayName(options_.api);
        // 使用已注册的 Unicode 窗口类创建顶层窗口，并保存返回的原生 HWND。
        window_ = CreateWindowExW(
            // 不启用 WS_EX_* 扩展样式。
            0,
            // 窗口类名称必须与 RegisterClassExW 注册时使用的名称完全一致。
            windowClass.lpszClassName,
            // Win32 在调用期间读取标题字符串，c_str() 提供以空字符结尾的宽字符指针。
            windowTitle.c_str(),
            // 使用前面选定的窗口样式，并用 WS_VISIBLE 让窗口创建后立即显示。
            windowStyle | WS_VISIBLE,
            // 窗口左上角的 X 坐标；窗口模式下为 CW_USEDEFAULT，全屏下为显示器左边界。
            windowX,
            // 窗口左上角的 Y 坐标；窗口模式下为 CW_USEDEFAULT，全屏下为显示器上边界。
            windowY,
            // 传入窗口总宽度；窗口模式已包含非客户区，全屏模式等于显示器宽度。
            windowWidth,
            // 传入窗口总高度；窗口模式已包含非客户区，全屏模式等于显示器高度。
            windowHeight,
            // 不设置父窗口，表示这是独立的顶层窗口。
            nullptr,
            // 不绑定菜单，也不把该参数用作子窗口 ID。
            nullptr,
            // 指定创建该窗口的可执行模块，与窗口类的 hInstance 保持一致。
            instance,
            // 把当前对象传给 WM_CREATE；WindowProcedure 从 CREATESTRUCT::lpCreateParams 取回并保存。
            this);
        // CreateWindowExW 失败时返回 nullptr，不能继续创建依赖 HWND 的 swapchain。
        if (window_ == nullptr) {
            // 立即报告窗口创建失败，阻止后续 RHI 初始化使用无效句柄。
            throw std::runtime_error("CreateWindowEx failed");
        }
    }

    /// 初始化所选 RHI 后端，并创建每帧 acquire/present 二进制信号。
    void CreateDevice(HINSTANCE instance) {
        // 公共 RHIDeviceCreateDesc 只描述“需要什么”；Vulkan 额外提供 surface 创建回调，
        // D3D 后端则直接从同一个 HWND 创建 DXGI swapchain，因此主循环不分后端。
        rhi::RHIDeviceCreateDesc desc{};
        desc.backend.applicationName = DEMO_APPLICATION_NAME;
        desc.backend.preferredApi = options_.api;
        desc.backend.validation = rhi::RHIValidationMode::Enabled;
        desc.backend.framesInFlight = FRAMES_IN_FLIGHT;
        desc.nativeWindow = window_;

        // Vulkan 必须由平台层提供 VkSurfaceKHR；D3D 后端直接使用上面的 HWND。
        // 将原生参数限制在对应 API 分支，避免公共 Demo 无意间依赖某个后端。
        if (options_.api == rhi::RHIGraphicsAPI::Vulkan) {
            desc.requiredVulkanInstanceExtensions = {
                VK_KHR_SURFACE_EXTENSION_NAME,
                VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
            desc.requiredVulkanDeviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
            desc.createVulkanSurface = [instance, window = window_](std::uintptr_t nativeInstance) {
                VkWin32SurfaceCreateInfoKHR surfaceInfo{};
                surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
                surfaceInfo.hinstance = instance;
                surfaceInfo.hwnd = window;
                VkSurfaceKHR surface = VK_NULL_HANDLE;
                if (vkCreateWin32SurfaceKHR(
                        reinterpret_cast<VkInstance>(nativeInstance),
                        &surfaceInfo,
                        nullptr,
                        &surface) != VK_SUCCESS) {
                    return std::uintptr_t{0};
                }
                return reinterpret_cast<std::uintptr_t>(surface);
            };
            desc.ownsVulkanSurface = true;
        }

        std::string error;
        device_ = rhi::CreateInitializedRHIDevice(desc, &error);
        if (device_ == nullptr) {
            throw std::runtime_error("RHI initialization failed: " + error);
        }

        for (rhi::u32 index = 0; index < FRAMES_IN_FLIGHT; ++index) {
            imageAvailable_[index] = device_->CreateGPUWaitGPUSignal(
                {"PBR.ImageAvailable" + std::to_string(index),
                 rhi::RHIGPUWaitGPUSignalType::Binary,
                 0});
            renderFinished_[index] = device_->CreateGPUWaitGPUSignal(
                {"PBR.RenderFinished" + std::to_string(index),
                 rhi::RHIGPUWaitGPUSignalType::Binary,
                 0});
        }
    }

    /// 仅在配置了 RenderDoc 版本中启用首帧显式捕获；正常构建不包含该依赖。
    void ConfigureRenderDocCapture() {
        if (!options_.renderDocCapture) {
            return;
        }
#if defined(PBRDEMO_ENABLE_RENDERDOC_CAPTURE)
        const HMODULE renderDocModule = GetModuleHandleA("renderdoc.dll");
        if (renderDocModule == nullptr) {
            throw std::runtime_error(
                "--renderdoc-capture requires launching through RenderDoc");
        }
        const auto getApi = reinterpret_cast<pRENDERDOC_GetAPI>(
            GetProcAddress(renderDocModule, "RENDERDOC_GetAPI"));
        void* api = nullptr;
        if (getApi == nullptr ||
            getApi(eRENDERDOC_API_Version_1_6_0, &api) == 0 || api == nullptr) {
            throw std::runtime_error("RenderDoc frame-capture API is unavailable");
        }
        renderDocApi_ = static_cast<RENDERDOC_API_1_6_0*>(api);
        renderDocCapturePending_ = true;
#else
        throw std::runtime_error(
            "--renderdoc-capture requires -DPBRDEMO_ENABLE_RENDERDOC_CAPTURE=ON");
#endif
    }

    /// 读取 metal_18 的五张贴图，创建可采样 GPU 纹理和共享 sampler。
    void CreateMaterialTextures() {
        // stb_image 在 CPU 端统一解码为 RGBA8，随后 RHI upload 只处理一种像素布局。
        // BaseColor 使用 sRGB 让采样自动回到线性空间；法线/金属度/粗糙度/高度保持 UNorm。
        const std::string materialDirectory =
            std::string(pbr_demo_config::ASSET_DIRECTORY) +
            "/metal_18-2K/";

        for (std::size_t index = 0; index < MATERIAL_TEXTURE_COUNT; ++index) {
            const MaterialTextureSpec& spec = MATERIAL_TEXTURE_SPECS[index];
            DecodedImage image = LoadImageRGBA8(materialDirectory + spec.fileName);
            MaterialTexture& material = materialTextures_[index];
            material.width = image.width;
            material.height = image.height;
            material.uploadData = std::move(image.pixels);

            rhi::RHITextureDesc textureDesc{};
            textureDesc.debugName = spec.debugName;
            textureDesc.dimension = rhi::RHITextureDimension::Texture2D;
            textureDesc.extent = {material.width, material.height, 1};
            textureDesc.format = spec.format;
            textureDesc.usage = rhi::RHITextureUsage::Sampled | rhi::RHITextureUsage::TransferDestination;
            material.texture = device_->CreateTexture(textureDesc);

            rhi::RHITextureViewDesc viewDesc{};
            viewDesc.debugName = std::string(spec.debugName) + ".View";
            viewDesc.texture = material.texture;
            viewDesc.dimension = rhi::RHITextureViewDimension::View2D;
            viewDesc.format = spec.format;
            viewDesc.aspect = rhi::RHITextureAspect::Color;
            material.view = device_->CreateTextureView(viewDesc);
        }

        rhi::RHISamplerDesc samplerDesc{};
        samplerDesc.debugName = "PBR.MetalSampler";
        samplerDesc.minFilter = rhi::RHIFilterMode::Linear;
        samplerDesc.magFilter = rhi::RHIFilterMode::Linear;
        samplerDesc.mipmapMode = rhi::RHIMipmapMode::Nearest;
        samplerDesc.addressU = rhi::RHIAddressMode::Repeat;
        samplerDesc.addressV = rhi::RHIAddressMode::Repeat;
        samplerDesc.addressW = rhi::RHIAddressMode::Repeat;
        samplerDesc.maxLod = 0.0F;
        materialSampler_ = device_->CreateSampler(samplerDesc);
    }

    /// 创建与窗口尺寸无关的几何、UBO、阴影、skybox、布局和绑定资源。
    void CreateStaticResources() {
        // 这里创建与窗口尺寸无关的对象：网格、材质和天空盒纹理、UBO、bind set layout。
        // swapchain 重建时这些对象无需重建，只有依赖颜色/深度格式的 pipeline 需要检查。
        const Mesh sphere = MakeSphere(32, 64);
        const Mesh plane = MakePlane();
        sphereVertexCount_ = static_cast<rhi::u32>(sphere.vertices.size());
        sphereIndexCount_ = static_cast<rhi::u32>(sphere.indices.size());
        planeIndexCount_ = static_cast<rhi::u32>(plane.indices.size());

        // 球体和地面合并进同一对 GPU buffer。地面 draw 通过 vertexOffsetElements
        // 跳过球体顶点，通过 planeIndexOffset_ 跳过球体索引。
        std::vector<Vertex> vertices = sphere.vertices;
        vertices.insert(vertices.end(), plane.vertices.begin(), plane.vertices.end());
        std::vector<rhi::u32> indices = sphere.indices;
        indices.insert(indices.end(), plane.indices.begin(), plane.indices.end());
        initialVertexData_ = ToBytes(vertices);
        initialIndexData_ = ToBytes(indices);
        sphereIndexOffset_ = 0;
        planeIndexOffset_ = static_cast<rhi::u64>(sphereIndexCount_) * sizeof(rhi::u32);

        rhi::RHIBufferDesc vertexDesc{};
        vertexDesc.debugName = "PBR.VertexBuffer";
        vertexDesc.size = initialVertexData_.size();
        vertexDesc.usage = rhi::RHIBufferUsage::Vertex | rhi::RHIBufferUsage::TransferDestination;
        vertexBuffer_ = device_->CreateBuffer(vertexDesc);

        rhi::RHIBufferDesc indexDesc{};
        indexDesc.debugName = "PBR.IndexBuffer";
        indexDesc.size = initialIndexData_.size();
        indexDesc.usage = rhi::RHIBufferUsage::Index | rhi::RHIBufferUsage::TransferDestination;
        indexBuffer_ = device_->CreateBuffer(indexDesc);

        rhi::RHIBufferDesc uniformDesc{};
        uniformDesc.size = sizeof(UniformBufferObject);
        uniformDesc.usage = rhi::RHIBufferUsage::Uniform | rhi::RHIBufferUsage::TransferDestination;
        uniformDesc.debugName = "PBR.SphereUniform";
        sphereUniformBuffer_ = device_->CreateBuffer(uniformDesc);
        uniformDesc.debugName = "PBR.PlaneUniform";
        planeUniformBuffer_ = device_->CreateBuffer(uniformDesc);

        CreateMaterialTextures();

        // 文件顺序必须与 cubemap array layer 约定一致，否则方向采样会看到错位或旋转的面。
        constexpr std::array<std::string_view, 6> faceFiles = {
            "px.png", "nx.png", "py.png", "ny.png", "pz.png", "nz.png"};
        const std::string skyboxDirectory =
            std::string(pbr_demo_config::ASSET_DIRECTORY) +
            "/sky_27_2k/sky_27_cubemap_2k/";
        for (std::size_t face = 0; face < faceFiles.size(); ++face) {
            DecodedImage image = LoadImageRGBA8(
                skyboxDirectory + std::string(faceFiles[face]));
            if (face == 0) {
                skyboxWidth_ = image.width;
                skyboxHeight_ = image.height;
                if (skyboxWidth_ != skyboxHeight_) {
                    throw std::runtime_error("Skybox faces must be square");
                }
            } else if (image.width != skyboxWidth_ || image.height != skyboxHeight_) {
                throw std::runtime_error("All skybox faces must have identical dimensions");
            }
            initialSkyboxFaceData_[face] = std::move(image.pixels);
        }

        // 环境图是显示颜色，所以使用 sRGB 格式让采样阶段自动解码到线性空间参与 PBR。
        // CubeCompatible 允许同一个 6-layer Texture2D 建立 Cube view。
        rhi::RHITextureDesc skyboxTextureDesc{};
        skyboxTextureDesc.debugName = "PBR.SkyboxCube";
        skyboxTextureDesc.dimension = rhi::RHITextureDimension::Texture2D;
        skyboxTextureDesc.extent = {skyboxWidth_, skyboxHeight_, 1};
        skyboxTextureDesc.arrayLayers = 6;
        skyboxTextureDesc.format = rhi::RHIFormat::RGBA8_SRGB;
        skyboxTextureDesc.usage = rhi::RHITextureUsage::Sampled | rhi::RHITextureUsage::TransferDestination;
        skyboxTextureDesc.flags = rhi::RHITextureCreateFlags::CubeCompatible;
        skyboxTexture_ = device_->CreateTexture(skyboxTextureDesc);

        rhi::RHITextureViewDesc skyboxViewDesc{};
        skyboxViewDesc.debugName = "PBR.SkyboxCubeView";
        skyboxViewDesc.texture = skyboxTexture_;
        skyboxViewDesc.dimension = rhi::RHITextureViewDimension::Cube;
        skyboxViewDesc.format = skyboxTextureDesc.format;
        skyboxViewDesc.aspect = rhi::RHITextureAspect::Color;
        skyboxViewDesc.arrayLayerCount = 6;
        skyboxView_ = device_->CreateTextureView(skyboxViewDesc);

        rhi::RHISamplerDesc skyboxSamplerDesc{};
        skyboxSamplerDesc.debugName = "PBR.SkyboxSampler";
        skyboxSamplerDesc.minFilter = rhi::RHIFilterMode::Linear;
        skyboxSamplerDesc.magFilter = rhi::RHIFilterMode::Linear;
        skyboxSamplerDesc.mipmapMode = rhi::RHIMipmapMode::Nearest;
        skyboxSamplerDesc.addressU = rhi::RHIAddressMode::ClampToEdge;
        skyboxSamplerDesc.addressV = rhi::RHIAddressMode::ClampToEdge;
        skyboxSamplerDesc.addressW = rhi::RHIAddressMode::ClampToEdge;
        skyboxSamplerDesc.maxLod = 0.0F;
        skyboxSampler_ = device_->CreateSampler(skyboxSamplerDesc);

        // binding 0 提供相机矩阵，由 Skybox Vertex Shader 去除观察平移；binding 2
        // 提供环境 cubemap。编号与 PBR layout 一致，方便 GLSL/HLSL 共享资源槽约定。
        rhi::RHIBindSetLayoutDesc skyboxBindLayoutDesc{};
        skyboxBindLayoutDesc.debugName = "PBR.SkyboxBindSetLayout";
        skyboxBindLayoutDesc.set = 0;
        skyboxBindLayoutDesc.entries.push_back({
            0,
            rhi::RHIBindingType::UniformBuffer,
            rhi::RHIShaderStage::Vertex | rhi::RHIShaderStage::Fragment});
        rhi::RHIBindSetLayoutEntry skyboxTextureEntry{};
        skyboxTextureEntry.binding = 2;
        skyboxTextureEntry.type = rhi::RHIBindingType::CombinedTextureSampler;
        skyboxTextureEntry.visibility = rhi::RHIShaderStage::Fragment;
        skyboxTextureEntry.textureViewDimension = rhi::RHITextureViewDimension::Cube;
        skyboxTextureEntry.textureSampleType = rhi::RHITextureSampleType::Float;
        skyboxBindLayoutDesc.entries.push_back(skyboxTextureEntry);
        skyboxBindSetLayout_ = device_->CreateBindSetLayout(skyboxBindLayoutDesc);

        rhi::RHIBindSetDesc skyboxBindSetDesc{};
        skyboxBindSetDesc.debugName = "PBR.SkyboxBindSet";
        skyboxBindSetDesc.layout = skyboxBindSetLayout_;
        rhi::RHIResourceBinding skyboxUniformBinding{};
        skyboxUniformBinding.binding = 0;
        skyboxUniformBinding.type = rhi::RHIBindingType::UniformBuffer;
        skyboxUniformBinding.buffer = { sphereUniformBuffer_, 0, sizeof(UniformBufferObject) };
        skyboxBindSetDesc.bindings.push_back(skyboxUniformBinding);
        rhi::RHIResourceBinding skyboxTextureBinding{};
        skyboxTextureBinding.binding = 2;
        skyboxTextureBinding.type = rhi::RHIBindingType::CombinedTextureSampler;
        skyboxTextureBinding.texture = {skyboxView_, skyboxTexture_};
        skyboxTextureBinding.sampler = skyboxSampler_;
        skyboxBindSetDesc.bindings.push_back(skyboxTextureBinding);
        skyboxBindSet_ = device_->CreateBindSet(skyboxBindSetDesc);

        rhi::RHIPipelineLayoutDesc skyboxPipelineLayoutDesc{};
        skyboxPipelineLayoutDesc.debugName = "PBR.SkyboxPipelineLayout";
        skyboxPipelineLayoutDesc.bindSetLayouts.push_back(skyboxBindSetLayout_);
        skyboxPipelineLayout_ = device_->CreatePipelineLayout(skyboxPipelineLayoutDesc);

        // Shadow Map 本质是一张“可采样的深度附件”：
        // - DepthStencilAttachment：允许 Shadow Pass 做深度测试并写入最近深度；
        // - Sampled：允许后续 PBR Fragment Shader 把它当只读纹理采样。
        // Vulkan 会据此组合 VkImageUsageFlags；D3D11/D3D12 后端会创建 typeless 资源，
        // 再分别建立 D32 DSV 与 R32_FLOAT SRV，使同一块显存支持两种解释方式。
        rhi::RHITextureDesc shadowTextureDesc{};
        shadowTextureDesc.debugName = "PBR.ShadowDepth";
        shadowTextureDesc.extent = {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 1};
        shadowTextureDesc.format = rhi::RHIFormat::D32_Float;
        shadowTextureDesc.usage = rhi::RHITextureUsage::DepthStencilAttachment | rhi::RHITextureUsage::Sampled;
        shadowTexture_ = device_->CreateTexture(shadowTextureDesc);

        // View 只暴露 depth aspect。这里没有 stencil，也不需要 color view。
        rhi::RHITextureViewDesc shadowViewDesc{};
        shadowViewDesc.debugName = "PBR.ShadowDepthView";
        shadowViewDesc.texture = shadowTexture_;
        shadowViewDesc.format = shadowTextureDesc.format;
        shadowViewDesc.aspect = rhi::RHITextureAspect::Depth;
        shadowView_ = device_->CreateTextureView(shadowViewDesc);

        // Comparison Sampler 不直接返回纹理中的深度，而是执行：
        //     referenceDepth <= storedDepth ? 1 : 0
        // Shader 传入当前片元的光源空间深度作为 referenceDepth；返回 1 表示没有被挡住。
        // Linear 过滤会在硬件支持时对邻近比较结果插值，再叠加 Shader 的 3x3 PCF，
        // 从而把锯齿状硬边变成较平滑的阴影边缘。
        rhi::RHISamplerDesc shadowSamplerDesc{};
        shadowSamplerDesc.debugName = "PBR.ShadowComparisonSampler";
        shadowSamplerDesc.minFilter = rhi::RHIFilterMode::Linear;
        shadowSamplerDesc.magFilter = rhi::RHIFilterMode::Linear;
        shadowSamplerDesc.mipmapMode = rhi::RHIMipmapMode::Nearest;
        shadowSamplerDesc.addressU = rhi::RHIAddressMode::ClampToBorder;
        shadowSamplerDesc.addressV = rhi::RHIAddressMode::ClampToBorder;
        shadowSamplerDesc.addressW = rhi::RHIAddressMode::ClampToBorder;
        shadowSamplerDesc.maxLod = 0.0F;
        shadowSamplerDesc.enableCompare = true;
        shadowSamplerDesc.compareOp = rhi::RHICompareOp::LessOrEqual;
        // 光源视锥外采到边框深度 1.0；标准深度下它代表最远处，因此默认判定为受光。
        shadowSamplerDesc.borderColor = rhi::RHIBorderColor::OpaqueWhite;
        shadowSampler_ = device_->CreateSampler(shadowSamplerDesc);

        // 主 PBR BindSet 的资源契约必须与 Shader 完全一致：
        // binding 0 -> 每个物体各自的 UniformBuffer；
        // binding 1 -> 全场景共享的 Shadow Map + Comparison Sampler；
        // binding 2 -> Skybox cubemap；binding 3..7 -> metal_18 PBR maps。
        // CombinedTextureSampler 在 Vulkan 对应 combined image sampler；D3D 后端会拆到
        // 同编号的 SRV(t1) 和 Sampler(s1)。
        rhi::RHIBindSetLayoutDesc bindLayoutDesc{};
        bindLayoutDesc.debugName = "PBR.BindSetLayout";
        bindLayoutDesc.set = 0;
        bindLayoutDesc.entries.push_back({
            0,
            rhi::RHIBindingType::UniformBuffer,
            rhi::RHIShaderStage::Vertex | rhi::RHIShaderStage::Fragment});

        rhi::RHIBindSetLayoutEntry shadowMapEntry{};
        shadowMapEntry.binding = 1;
        shadowMapEntry.type = rhi::RHIBindingType::CombinedTextureSampler;
        shadowMapEntry.visibility = rhi::RHIShaderStage::Fragment;
        shadowMapEntry.textureViewDimension = rhi::RHITextureViewDimension::View2D;
        shadowMapEntry.textureSampleType = rhi::RHITextureSampleType::Depth;
        bindLayoutDesc.entries.push_back(shadowMapEntry);
        rhi::RHIBindSetLayoutEntry pbrSkyboxEntry{};
        pbrSkyboxEntry.binding = 2;
        pbrSkyboxEntry.type = rhi::RHIBindingType::CombinedTextureSampler;
        pbrSkyboxEntry.visibility = rhi::RHIShaderStage::Fragment;
        pbrSkyboxEntry.textureViewDimension = rhi::RHITextureViewDimension::Cube;
        pbrSkyboxEntry.textureSampleType = rhi::RHITextureSampleType::Float;
        bindLayoutDesc.entries.push_back(pbrSkyboxEntry);
        for (std::size_t index = 0; index < MATERIAL_TEXTURE_COUNT; ++index) {
            rhi::RHIBindSetLayoutEntry materialEntry{};
            materialEntry.binding = 3U + static_cast<rhi::u32>(index);
            materialEntry.type = rhi::RHIBindingType::CombinedTextureSampler;
            materialEntry.visibility = rhi::RHIShaderStage::Fragment;
            materialEntry.textureViewDimension = rhi::RHITextureViewDimension::View2D;
            materialEntry.textureSampleType = rhi::RHITextureSampleType::Float;
            bindLayoutDesc.entries.push_back(materialEntry);
        }
        bindSetLayout_ = device_->CreateBindSetLayout(bindLayoutDesc);

        sphereBindSet_ = CreatePBRBindSet("PBR.SphereBindSet", sphereUniformBuffer_);
        planeBindSet_ = CreatePBRBindSet("PBR.PlaneBindSet", planeUniformBuffer_);

        rhi::RHIPipelineLayoutDesc pipelineLayoutDesc{};
        pipelineLayoutDesc.debugName = "PBR.PipelineLayout";
        pipelineLayoutDesc.bindSetLayouts.push_back(bindSetLayout_);
        pipelineLayout_ = device_->CreatePipelineLayout(pipelineLayoutDesc);

        // Depth-only Shadow Pipeline 的布局只有 binding 0。它只需要 model 和
        // lightViewProjection，不会访问 binding 1 的 Shadow Map。
        rhi::RHIBindSetLayoutDesc shadowBindLayoutDesc{};
        shadowBindLayoutDesc.debugName = "PBR.ShadowBindSetLayout";
        shadowBindLayoutDesc.set = 0;
        shadowBindLayoutDesc.entries.push_back({
            0,
            rhi::RHIBindingType::UniformBuffer,
            rhi::RHIShaderStage::Vertex});
        shadowBindSetLayout_ = device_->CreateBindSetLayout(shadowBindLayoutDesc);
        shadowSphereBindSet_ = CreateShadowBindSet(
            "PBR.ShadowSphereBindSet",
            sphereUniformBuffer_);

        rhi::RHIPipelineLayoutDesc shadowPipelineLayoutDesc{};
        shadowPipelineLayoutDesc.debugName = "PBR.ShadowPipelineLayout";
        shadowPipelineLayoutDesc.bindSetLayouts.push_back(shadowBindSetLayout_);
        shadowPipelineLayout_ = device_->CreatePipelineLayout(shadowPipelineLayoutDesc);
    }

    /// 为一个物体绑定独立 UBO，并复用全场景阴影、环境和 metal_18 贴图。
    rhi::RHIBindSet CreatePBRBindSet(const char* name, rhi::RHIBuffer buffer) {
        // 球和 Plane 各有自己的 UBO（model、材质不同），但共享同一张阴影图。
        // BindSet 把“这个 draw 实际使用哪些资源”与 Pipeline 的静态布局分离。
        rhi::RHIBindSetDesc desc{};
        desc.debugName = name;
        desc.layout = bindSetLayout_;

        rhi::RHIResourceBinding uniformBinding{};
        uniformBinding.binding = 0;
        uniformBinding.type = rhi::RHIBindingType::UniformBuffer;
        uniformBinding.buffer = {buffer, 0, sizeof(UniformBufferObject)};
        desc.bindings.push_back(uniformBinding);

        rhi::RHIResourceBinding shadowBinding{};
        shadowBinding.binding = 1;
        shadowBinding.type = rhi::RHIBindingType::CombinedTextureSampler;
        shadowBinding.texture = {shadowView_, shadowTexture_};
        shadowBinding.sampler = shadowSampler_;
        desc.bindings.push_back(shadowBinding);

        rhi::RHIResourceBinding skyboxBinding{};
        skyboxBinding.binding = 2;
        skyboxBinding.type = rhi::RHIBindingType::CombinedTextureSampler;
        skyboxBinding.texture = {skyboxView_, skyboxTexture_};
        skyboxBinding.sampler = skyboxSampler_;
        desc.bindings.push_back(skyboxBinding);

        for (std::size_t index = 0; index < MATERIAL_TEXTURE_COUNT; ++index) {
            const MaterialTexture& material = materialTextures_[index];
            rhi::RHIResourceBinding materialBinding{};
            materialBinding.binding = 3U + static_cast<rhi::u32>(index);
            materialBinding.type = rhi::RHIBindingType::CombinedTextureSampler;
            materialBinding.texture = {material.view, material.texture};
            materialBinding.sampler = materialSampler_;
            desc.bindings.push_back(materialBinding);
        }
        return device_->CreateBindSet(desc);
    }

    /// 为 Shadow Pass 创建只包含物体 UBO 的绑定，避免边写边采样 shadowTexture_。
    rhi::RHIBindSet CreateShadowBindSet(const char* name, rhi::RHIBuffer buffer) {
        // Shadow Pass 当前只有球体充当 caster，所以只创建球体 BindSet。
        // Plane 是 receiver，不写入 Shadow Map；否则它只会写下自身平面深度，
        // 对“球是否挡住光线”的判断没有额外帮助，并增加一次无意义绘制。
        rhi::RHIBindSetDesc desc{};
        desc.debugName = name;
        desc.layout = shadowBindSetLayout_;

        rhi::RHIResourceBinding uniformBinding{};
        uniformBinding.binding = 0;
        uniformBinding.type = rhi::RHIBindingType::UniformBuffer;
        uniformBinding.buffer = {buffer, 0, sizeof(UniformBufferObject)};
        desc.bindings.push_back(uniformBinding);
        return device_->CreateBindSet(desc);
    }

    /// 查询当前 Win32 客户区尺寸；窗口最小化时允许返回 0x0。
    rhi::RHIExtent2D ClientExtent() const {
        RECT rectangle{};
        GetClientRect(window_, &rectangle);
        return {
            static_cast<rhi::u32>(std::max<LONG>(0, rectangle.right - rectangle.left)),
            static_cast<rhi::u32>(std::max<LONG>(0, rectangle.bottom - rectangle.top))};
    }

    /// 创建随窗口尺寸变化的 swapchain、主深度资源，并按需创建图形管线。
    void CreateSwapchainResources() {
        // swapchain 是窗口尺寸的唯一源头。深度纹理和 pipeline attachment format 必须
        // 与它同步；最小化时客户区为 0，暂缓创建以避免无效 extent。
        const rhi::RHIExtent2D extent = ClientExtent();
        if (extent.width == 0 || extent.height == 0) {
            return;
        }

        rhi::RHISwapchainDesc swapchainDesc{};
        swapchainDesc.debugName = "PBR.Swapchain";
        swapchainDesc.extent = extent;
        swapchainDesc.preferredFormat = rhi::RHIFormat::BGRA8_SRGB;
        swapchainDesc.presentMode = rhi::RHIPresentMode::FIFO;
        swapchainDesc.imageCount = FRAMES_IN_FLIGHT + 1;
        swapchain_ = device_->CreateSwapchain(swapchainDesc);
        swapchainImages_ = device_->GetSwapchainImages(swapchain_);
        swapchainFormat_ = device_->GetSwapchainFormat(swapchain_);
        swapchainExtent_ = device_->GetSwapchainExtent(swapchain_);

        rhi::RHITextureDesc depthDesc{};
        depthDesc.debugName = "PBR.Depth";
        depthDesc.extent = {swapchainExtent_.width, swapchainExtent_.height, 1};
        depthDesc.format = rhi::RHIFormat::D32_Float;
        depthDesc.usage = rhi::RHITextureUsage::DepthStencilAttachment;
        depthTexture_ = device_->CreateTexture(depthDesc);

        rhi::RHITextureViewDesc depthViewDesc{};
        depthViewDesc.debugName = "PBR.DepthView";
        depthViewDesc.texture = depthTexture_;
        depthViewDesc.format = depthDesc.format;
        depthViewDesc.aspect = rhi::RHITextureAspect::Depth;
        depthView_ = device_->CreateTextureView(depthViewDesc);

        if (!pipeline_) {
            CreatePipeline();
        }
    }

    /// 根据当前后端选择 SPIR-V/HLSL，并创建 PBR、Shadow 和 Skybox 三条管线。
    void CreatePipeline() {
        // 一个公共描述分别落成三套原生 PSO：Vulkan 使用预编译 SPIR-V，D3D 使用 HLSL
        // 入口点。shadow 是 depth-only，skybox 使用 LessEqual 且关闭深度写入。
        rhi::RHIShaderDesc vertexShader{};
        vertexShader.debugName = "PBR.VertexShader";
        vertexShader.stage = rhi::RHIShaderStage::Vertex;

        rhi::RHIShaderDesc fragmentShader{};
        fragmentShader.debugName = "PBR.FragmentShader";
        fragmentShader.stage = rhi::RHIShaderStage::Fragment;

        const std::string shaderDirectory = pbr_demo_config::SHADER_DIRECTORY;
        if (options_.api == rhi::RHIGraphicsAPI::Vulkan) {
            vertexShader.language = rhi::RHIShaderLanguage::SPIRV;
            vertexShader.filePath = shaderDirectory + "/pbr.vert.spv";
            fragmentShader.language = rhi::RHIShaderLanguage::SPIRV;
            fragmentShader.filePath = shaderDirectory + "/pbr.frag.spv";
        } else {
            // D3D11 与 D3D12 共用 HLSL 源码，但分别编译到各自合适的 shader model。
            // b0、POSITION/NORMAL/TEXCOORD 和 SV_POSITION/SV_TARGET 共同构成 D3D 管线契约。
            const bool d3d12 = options_.api == rhi::RHIGraphicsAPI::D3D12;
            vertexShader.language = rhi::RHIShaderLanguage::HLSL;
            vertexShader.filePath = shaderDirectory + "/pbr.hlsl";
            vertexShader.entryPoint = "VSMain";
            vertexShader.compileOptions.targetProfile = d3d12 ? "vs_5_1" : "vs_5_0";
            fragmentShader.language = rhi::RHIShaderLanguage::HLSL;
            fragmentShader.filePath = shaderDirectory + "/pbr.hlsl";
            fragmentShader.entryPoint = "PSMain";
            fragmentShader.compileOptions.targetProfile = d3d12 ? "ps_5_1" : "ps_5_0";
        }

        rhi::RHIVertexBufferLayoutDesc vertexLayout{};
        vertexLayout.binding = 0;
        vertexLayout.stride = sizeof(Vertex);
        vertexLayout.attributes = {
            {"POSITION", 0, 0, 0, rhi::RHIVertexFormat::Float32x3, offsetof(Vertex, position)},
            {"NORMAL", 0, 1, 0, rhi::RHIVertexFormat::Float32x3, offsetof(Vertex, normal)},
            {"TEXCOORD", 0, 2, 0, rhi::RHIVertexFormat::Float32x2, offsetof(Vertex, uv)},
            {"TANGENT", 0, 3, 0, rhi::RHIVertexFormat::Float32x4, offsetof(Vertex, tangent)}};

        rhi::RHIGraphicsPipelineDesc pipelineDesc{};
        pipelineDesc.debugName = "PBR.GraphicsPipeline";
        pipelineDesc.layout = pipelineLayout_;
        pipelineDesc.shaders = {vertexShader, fragmentShader};
        pipelineDesc.vertexBuffers.push_back(vertexLayout);
        pipelineDesc.inputAssembly.topology = rhi::RHIPrimitiveTopology::TriangleList;
        pipelineDesc.raster.cullMode = rhi::RHICullMode::Back;
        pipelineDesc.raster.frontFace = rhi::RHIFrontFace::Clockwise;
        pipelineDesc.depthStencil.depthTestEnable = true;
        pipelineDesc.depthStencil.depthWriteEnable = true;
        pipelineDesc.depthStencil.depthCompareOp = rhi::RHICompareOp::Less;
        pipelineDesc.blend.attachments.push_back({});
        pipelineDesc.colorFormats.push_back(swapchainFormat_);
        pipelineDesc.depthStencilFormat = rhi::RHIFormat::D32_Float;
        pipeline_ = device_->CreateGraphicsPipeline(pipelineDesc);

        // Shadow Pipeline 只包含 Vertex Shader：顶点经过 model 和光源 VP 矩阵后，
        // 固定功能光栅器会自动生成片元深度并写入 D32 attachment。没有颜色输出，
        // 所以不需要 Fragment/Pixel Shader，也不声明 colorFormats。
        rhi::RHIShaderDesc shadowVertexShader{};
        shadowVertexShader.debugName = "PBR.ShadowVertexShader";
        shadowVertexShader.stage = rhi::RHIShaderStage::Vertex;
        if (options_.api == rhi::RHIGraphicsAPI::Vulkan) {
            shadowVertexShader.language = rhi::RHIShaderLanguage::SPIRV;
            shadowVertexShader.filePath = shaderDirectory + "/shadow.vert.spv";
        } else {
            const bool d3d12 = options_.api == rhi::RHIGraphicsAPI::D3D12;
            shadowVertexShader.language = rhi::RHIShaderLanguage::HLSL;
            shadowVertexShader.filePath = shaderDirectory + "/pbr.hlsl";
            shadowVertexShader.entryPoint = "ShadowVSMain";
            shadowVertexShader.compileOptions.targetProfile = d3d12 ? "vs_5_1" : "vs_5_0";
        }

        // Shadow Shader 只读取 POSITION，但 stride 仍是 sizeof(Vertex)：
        // position/normal/uv 在同一交错顶点中，下一顶点仍需跨过完整 Vertex。
        // 省略 NORMAL 和 UV attribute 只会减少输入布局声明，不会改变内存步长。
        rhi::RHIVertexBufferLayoutDesc shadowVertexLayout{};
        shadowVertexLayout.binding = 0;
        shadowVertexLayout.stride = sizeof(Vertex);
        shadowVertexLayout.attributes = {
            {"POSITION", 0, 0, 0, rhi::RHIVertexFormat::Float32x3, offsetof(Vertex, position)}};

        rhi::RHIGraphicsPipelineDesc shadowPipelineDesc{};
        shadowPipelineDesc.debugName = "PBR.ShadowGraphicsPipeline";
        shadowPipelineDesc.layout = shadowPipelineLayout_;
        shadowPipelineDesc.shaders = {shadowVertexShader};
        shadowPipelineDesc.vertexBuffers.push_back(shadowVertexLayout);
        shadowPipelineDesc.inputAssembly.topology = rhi::RHIPrimitiveTopology::TriangleList;
        shadowPipelineDesc.raster.cullMode = rhi::RHICullMode::Back;
        shadowPipelineDesc.raster.frontFace = rhi::RHIFrontFace::Clockwise;
        // Shadow acne 的来源：有限深度精度和光栅化采样位置会让接收面与自己写入的深度
        // 略有误差，从而被错误判断为“自己遮挡自己”。Raster Depth Bias 将 caster
        // 写入的深度轻微推远；Fragment Shader 还会在比较前使用法线相关 bias。
        // bias 太小会出现条纹，太大则会产生阴影与物体分离的 Peter-panning。
        shadowPipelineDesc.raster.depthBiasEnable = true;
        shadowPipelineDesc.raster.depthBiasConstantFactor = 1.0F;
        shadowPipelineDesc.raster.depthBiasSlopeFactor = 1.5F;
        shadowPipelineDesc.depthStencil.depthTestEnable = true;
        shadowPipelineDesc.depthStencil.depthWriteEnable = true;
        shadowPipelineDesc.depthStencil.depthCompareOp = rhi::RHICompareOp::Less;
        shadowPipelineDesc.depthStencilFormat = rhi::RHIFormat::D32_Float;
        shadowPipeline_ = device_->CreateGraphicsPipeline(shadowPipelineDesc);

        // Skybox 复用球体 POSITION 作为方向。Vertex Shader 把深度固定在远平面，
        // LessOrEqual 且关闭深度写入，使它只填充尚未被场景几何覆盖的背景像素。
        rhi::RHIShaderDesc skyboxVertexShader{};
        skyboxVertexShader.debugName = "PBR.SkyboxVertexShader";
        skyboxVertexShader.stage = rhi::RHIShaderStage::Vertex;
        rhi::RHIShaderDesc skyboxFragmentShader{};
        skyboxFragmentShader.debugName = "PBR.SkyboxFragmentShader";
        skyboxFragmentShader.stage = rhi::RHIShaderStage::Fragment;
        if (options_.api == rhi::RHIGraphicsAPI::Vulkan) {
            skyboxVertexShader.language = rhi::RHIShaderLanguage::SPIRV;
            skyboxVertexShader.filePath = shaderDirectory + "/skybox.vert.spv";
            skyboxFragmentShader.language = rhi::RHIShaderLanguage::SPIRV;
            skyboxFragmentShader.filePath = shaderDirectory + "/skybox.frag.spv";
        } else {
            const bool d3d12 = options_.api == rhi::RHIGraphicsAPI::D3D12;
            skyboxVertexShader.language = rhi::RHIShaderLanguage::HLSL;
            skyboxVertexShader.filePath = shaderDirectory + "/pbr.hlsl";
            skyboxVertexShader.entryPoint = "SkyboxVSMain";
            skyboxVertexShader.compileOptions.targetProfile = d3d12 ? "vs_5_1" : "vs_5_0";
            skyboxFragmentShader.language = rhi::RHIShaderLanguage::HLSL;
            skyboxFragmentShader.filePath = shaderDirectory + "/pbr.hlsl";
            skyboxFragmentShader.entryPoint = "SkyboxPSMain";
            skyboxFragmentShader.compileOptions.targetProfile = d3d12 ? "ps_5_1" : "ps_5_0";
        }

        rhi::RHIGraphicsPipelineDesc skyboxPipelineDesc{};
        skyboxPipelineDesc.debugName = "PBR.SkyboxGraphicsPipeline";
        skyboxPipelineDesc.layout = skyboxPipelineLayout_;
        skyboxPipelineDesc.shaders = {skyboxVertexShader, skyboxFragmentShader};
        skyboxPipelineDesc.vertexBuffers.push_back(shadowVertexLayout);
        skyboxPipelineDesc.inputAssembly.topology = rhi::RHIPrimitiveTopology::TriangleList;
        skyboxPipelineDesc.raster.cullMode = rhi::RHICullMode::None;
        skyboxPipelineDesc.raster.frontFace = rhi::RHIFrontFace::CounterClockwise;
        skyboxPipelineDesc.depthStencil.depthTestEnable = true;
        skyboxPipelineDesc.depthStencil.depthWriteEnable = false;
        skyboxPipelineDesc.depthStencil.depthCompareOp = rhi::RHICompareOp::LessOrEqual;
        skyboxPipelineDesc.blend.attachments.push_back({});
        skyboxPipelineDesc.colorFormats.push_back(swapchainFormat_);
        skyboxPipelineDesc.depthStencilFormat = rhi::RHIFormat::D32_Float;
        skyboxPipeline_ = device_->CreateGraphicsPipeline(skyboxPipelineDesc);
    }

    /// 等待旧呈现资源空闲后重建窗口尺寸相关资源；静态场景资源保持不变。
    void RecreateSwapchain() {
        // 等 GPU 空闲后才能销毁旧 backbuffer/depth/view。UI 也依赖旧颜色格式，
        // 所以先销毁 UI，再创建新 swapchain，最后用新格式重建 UI pipeline。
        const rhi::RHIExtent2D extent = ClientExtent();
        if (extent.width == 0 || extent.height == 0) {
            return;
        }
        device_->WaitIdle();
        if (ui_ != nullptr) {
            ui_->Shutdown();
            ui_.reset();
        }
        device_->Destroy(depthView_);
        device_->Destroy(depthTexture_);
        device_->Destroy(swapchain_);
        depthView_ = {};
        depthTexture_ = {};
        swapchain_ = {};
        swapchainImages_.clear();
        CreateSwapchainResources();
        CreateUI();
        framebufferResized_ = false;
    }

    /// Recreate the UI with the active swapchain format using only public RHI objects.
    void CreateUI() {
        // Context 不持有 swapchain，只接收颜色/深度格式来创建兼容的 UI pipeline；
        // 这正是“无需修改 RHI 核心也能加 UI”的边界。
        ui_ = std::make_unique<rhi::ui::Context>(
            *device_,
            swapchainFormat_,
            rhi::RHIFormat::D32_Float,
            pbr_demo_config::SHADER_DIRECTORY);
    }

    /// Builds the demo overlay. The UI pass follows PBR, and layer values control overlap
    /// within the overlay itself.
    void BuildUI() {
        // 每帧按稳定顺序提交控件：面板 -> 选择按钮 -> 贴图预览 -> 旋转滑块。
        // layer 只解决 UI 内部遮挡，UI pass 本身由 RenderGraph 排在不透明场景之后。
        if (ui_ == nullptr) {
            return;
        }

        ui_->BeginFrame(swapchainExtent_, uiInput_);
        constexpr rhi::ui::Rect panelRect{20.0F, 20.0F, 356.0F, 590.0F};
        constexpr std::array<std::string_view, MATERIAL_TEXTURE_COUNT> textureLabels = {
            "BASE COLOR", "NORMAL", "METALLIC", "ROUGHNESS", "HEIGHT"};

        ui_->Panel(panelRect, {0.025F, 0.045F, 0.065F, 0.91F}, 10);
        ui_->TextBox(
            {38.0F, 38.0F, 320.0F, 31.0F},
            "PBR MATERIAL INSPECTOR",
            {0.06F, 0.15F, 0.20F, 0.98F},
            {0.75F, 0.94F, 1.0F, 1.0F},
            11);

        for (std::size_t index = 0; index < MATERIAL_TEXTURE_COUNT; ++index) {
            const float buttonY = 84.0F + static_cast<float>(index) * 38.0F;
            const rhi::ui::Rect buttonRect{38.0F, buttonY, 142.0F, 30.0F};
            if (selectedMaterialTexture_ == index) {
                ui_->Panel(
                    {buttonRect.x - 2.0F, buttonRect.y - 2.0F,
                     buttonRect.width + 4.0F, buttonRect.height + 4.0F},
                    {0.25F, 0.78F, 0.91F, 0.96F},
                    12);
            }
            if (ui_->Button(textureLabels[index], buttonRect, 13)) {
                selectedMaterialTexture_ = index;
            }
        }

        const MaterialTexture& selectedTexture = materialTextures_[selectedMaterialTexture_];
        const MaterialTextureSpec& selectedSpec = MATERIAL_TEXTURE_SPECS[selectedMaterialTexture_];
        ui_->TextBox(
            {198.0F, 84.0F, 160.0F, 30.0F},
            textureLabels[selectedMaterialTexture_],
            {0.06F, 0.15F, 0.20F, 0.98F},
            {0.75F, 0.94F, 1.0F, 1.0F},
            14);
        ui_->Image(
            {198.0F, 128.0F, 150.0F, 150.0F},
            selectedTexture.texture,
            selectedTexture.view,
            selectedSpec.graphName,
            14);
        (void)ui_->SliderFloat(
            "SPHERE ROTATION",
            {38.0F, 308.0F, 310.0F, 34.0F},
            sphereRotationDegreesPerSecond_,
            0.0F,
            180.0F,
            1.0F,
            14);
        (void)ui_->SliderFloat(
            "SKY ROTATION",
            {38.0F, 350.0F, 310.0F, 34.0F},
            skyRotationDegreesPerSecond_,
            0.0F,
            60.0F,
            0.5F,
            14);
        ui_->TextBox(
            {38.0F, 407.0F, 310.0F, 30.0F},
            "METAL 18 MATERIAL",
            {0.045F, 0.09F, 0.12F, 0.96F},
            {0.52F, 0.80F, 0.90F, 1.0F},
            14);

        uiInput_.leftButtonPressed = false;
        uiInput_.leftButtonReleased = false;
    }

    /// 为球体或地面生成当前帧的相机、光源、阴影和材质常量。
    UniformBufferObject MakeUniform(bool sphere) const {
        // sphere 参数决定 model 是否加入旋转；其余相机、光照、shadow 矩阵保持一致，
        // 因而材质切换只需替换 bind set 中的纹理，不会改变网格或渲染图拓扑。
        const float time = std::chrono::duration<float>(
                               std::chrono::steady_clock::now() - startTime_)
                               .count();
        const float3 eye = cameraPosition_;

        UniformBufferObject uniform{};
        uniform.view = ToShaderMatrix(LookAtLH(
            eye,
            eye + CameraForward(),
            float3{0.0F, 1.0F, 0.0F}));
        float4x4 projection = PerspectiveLH_ZO(
            math::Radians(45.0F),
            static_cast<float>(swapchainExtent_.width) /
                static_cast<float>(swapchainExtent_.height),
            0.1F,
            100.0F);
        // LH ZO 投影可直接配合 D3D 的 viewport Y 方向；Vulkan 的正高度 viewport
        // 需要翻转 clip-space Y。若 D3D 也翻转，画面会上下颠倒且三角形屏幕绕序反转。
        if (options_.api == rhi::RHIGraphicsAPI::Vulkan) {
            projection[1][1] *= -1.0F;
        }
        uniform.projection = ToShaderMatrix(projection);
        // Preserve the light's physical side after the RH-to-LH Z reflection.
        const float3 normalizedLightDirection = Normalize(float3{-0.5F, -1.0F, 0.3F});
        uniform.lightDirection = {
            normalizedLightDirection.x,
            normalizedLightDirection.y,
            normalizedLightDirection.z,
            0.0F};
        uniform.lightColor = {1.0F, 0.96F, 0.90F, 1.0F};
        uniform.cameraPosition = {eye.x, eye.y, eye.z, 1.0F};

        // 方向光没有真实位置，所有光线互相平行。为了生成 Shadow Map，仍需构造一个
        // “虚拟光源相机”：把它放在光线传播方向的反方向，并朝场景中心观察。
        //
        // lightPosition = target - lightDirection * distance
        // 因为 lightDirection 指向光线前进方向，减去它才会回到光线来源一侧。
        const float3 lightDirection{
            uniform.lightDirection.x,
            uniform.lightDirection.y,
            uniform.lightDirection.z};
        const float3 shadowTarget{0.0F, 0.5F, 0.0F};
        const float3 lightPosition = shadowTarget - lightDirection * 8.0F;
        const float4x4 lightView = LookAtLH(
            lightPosition,
            shadowTarget,
            float3{0.0F, 1.0F, 0.0F});

        // 方向光没有“近大远小”，所以使用正交投影而不是透视投影。
        // left/right/bottom/top 决定 Shadow Map 覆盖的世界区域；范围过大时，每个 texel
        // 覆盖更多世界空间，阴影会变糊；范围过小时，范围外物体不会进入阴影图。
        // near/far 决定光源方向上的可记录深度范围，也应尽量贴合场景以提高精度。
        float4x4 lightProjection = OrthographicLH_ZO(
            -5.0F,
            5.0F,
            -5.0F,
            5.0F,
            0.1F,
            16.0F);

        // 主相机和光源相机必须采用相同的后端 Y 约定。这里为 Vulkan 翻转光源投影 Y，
        // 因此 GLSL 查询阴影时可以直接执行 ndc.xy * 0.5 + 0.5；D3D 不翻矩阵，
        // 转而在 HLSL 计算 shadowUV 时翻转 Y。
        if (options_.api == rhi::RHIGraphicsAPI::Vulkan) {
            lightProjection[1][1] *= -1.0F;
        }

        // 顶点先 world = model * local，再 lightClip = lightVP * world。
        // model 因物体而异，lightVP 对同一个方向光覆盖的所有物体相同。
        uniform.lightViewProjection = ToShaderMatrix(lightProjection * lightView);

        // PCF 每次偏移一个 texel，因此把 1 / resolution 传给 Shader，避免硬编码。
        // z/w 是经过实际画面调节的比较 bias，单位是归一化光源深度 [0, 1]。
        uniform.shadowParameters = {
            1.0F / static_cast<float>(SHADOW_MAP_SIZE),
            1.0F / static_cast<float>(SHADOW_MAP_SIZE),
            0.00035F,
            0.0025F};

        if (sphere) {
            uniform.model = ToShaderMatrix(
                TranslationMatrix(float3{0.0F, 1.0F, 0.0F}) *
                // RotationYMatrix is RH positive-angle; negate it for LH positive Y rotation.
                RotationYMatrix(-time * math::Radians(sphereRotationDegreesPerSecond_)));
            // 金属球直接呈现 metal_18 的原始 base color、metallic、roughness、normal
            // 与 height 信息；alpha/x 分别作为金属度和粗糙度贴图的可调乘数。
            uniform.baseColor = {1.0F, 1.0F, 1.0F, 1.0F};
            uniform.materialParameters = {
                1.0F,
                1.0F,
                0.045F,
                time * math::Radians(skyRotationDegreesPerSecond_)};
        } else {
            uniform.model = ToShaderMatrix(float4x4{1.0F});
            // 地面复用同一组贴图但做暗色、低金属度调制，并通过 4x UV 平铺展示细节。
            uniform.baseColor = {0.38F, 0.42F, 0.46F, 0.12F};
            // 地面模型和环境采样保持固定，天空旋转时地面不会产生旋转错觉。
            uniform.materialParameters = {0.90F, 1.0F, 0.018F, 0.0F};
        }
        return uniform;
    }

    /// 将资源上传、RenderGraph、draw workload、队列同步和 present 组装为一帧 packet。
    // 构造 RenderGraph 的声明部分和对应 workload。声明先于命令提交，使图编译器
    // 能从 reads/attachments 推导 pass 顺序与跨 API 的资源状态转换。
    rhi::RHIFramePacket BuildFrame(rhi::u32 imageIndex) {
        rhi::RHIFramePacket packet{};
        // ForwardRenderPipeline 不拥有任何资源；它只接收下面声明的 pass/workload，
        // 在提交前统一写入 packet，固定前向渲染阶段的可读顺序。
        rhi::pipeline::ForwardRenderPipeline forwardPipeline{};
        packet.settings.drawableSize = swapchainExtent_;
        packet.settings.viewport = {
            0.0F,
            0.0F,
            static_cast<float>(swapchainExtent_.width),
            static_cast<float>(swapchainExtent_.height),
            0.0F,
            1.0F};
        packet.settings.scissor = {{0, 0}, swapchainExtent_};
        packet.settings.frameIndex = frameIndex_;
        packet.settings.maxFramesInFlight = FRAMES_IN_FLIGHT;

        // Build controls before making UBOs so slider changes affect this submitted frame.
        BuildUI();

        // 静态几何、skybox 和 metal_18 贴图只上传一次；UBO 则因动画和相机参数每帧更新。
        if (staticUploadsPending_) {
            packet.uploads.buffers.push_back({vertexBuffer_, 0, initialVertexData_});
            packet.uploads.buffers.push_back({indexBuffer_, 0, initialIndexData_});
            for (rhi::u32 face = 0; face < initialSkyboxFaceData_.size(); ++face) {
                rhi::RHITextureUploadDesc upload{};
                upload.destination = skyboxTexture_;
                upload.arrayLayer = face;
                upload.extent = {skyboxWidth_, skyboxHeight_, 1};
                upload.data = initialSkyboxFaceData_[face];
                packet.uploads.textures.push_back(std::move(upload));
            }
            for (std::size_t index = 0; index < MATERIAL_TEXTURE_COUNT; ++index) {
                const MaterialTexture& material = materialTextures_[index];
                rhi::RHITextureUploadDesc upload{};
                upload.destination = material.texture;
                upload.extent = {material.width, material.height, 1};
                // stb_image 输出连续 RGBA8 像素；保持 0 让 RHI 按目标格式自动推导
                // 紧密 row pitch，这也满足当前 Vulkan 上传路径的限制。
                upload.data = material.uploadData;
                packet.uploads.textures.push_back(std::move(upload));
            }
        }
        packet.uploads.buffers.push_back(
            {sphereUniformBuffer_, 0, ToBytes(MakeUniform(true))});
        packet.uploads.buffers.push_back(
            {planeUniformBuffer_, 0, ToBytes(MakeUniform(false))});
        if (ui_ != nullptr) {
            ui_->AppendUploads(packet);
        }

        // RenderGraph 只引用长期存在的外部 RHI 句柄，不接管这些 buffer 的生命周期。
        const auto importBuffer = [&](
            const char* name,
            rhi::RHIBuffer handle,
            rhi::u64 size,
            rhi::RHIBufferUsage usage) {
            rhi::RHIRenderGraphBufferDesc resource{};
            resource.name = name;
            resource.imported = true;
            resource.flags = rhi::RHIRenderGraphResourceFlags::Imported;
            resource.externalHandle = handle;
            resource.desc.size = size;
            resource.desc.usage = usage;
            packet.graph.buffers.push_back(resource);
        };
        importBuffer("Vertices", vertexBuffer_, initialVertexData_.size(), rhi::RHIBufferUsage::Vertex);
        importBuffer("Indices", indexBuffer_, initialIndexData_.size(), rhi::RHIBufferUsage::Index);
        importBuffer("SphereUniform", sphereUniformBuffer_, sizeof(UniformBufferObject), rhi::RHIBufferUsage::Uniform);
        importBuffer("PlaneUniform", planeUniformBuffer_, sizeof(UniformBufferObject), rhi::RHIBufferUsage::Uniform);

        // swapchain image 和主深度均由图外创建，本帧只声明它们的用途和初始状态。
        rhi::RHIRenderGraphTextureDesc backBuffer{};
        backBuffer.name = "BackBuffer";
        backBuffer.imported = true;
        backBuffer.flags = rhi::RHIRenderGraphResourceFlags::Imported;
        backBuffer.externalHandle = swapchainImages_[imageIndex];
        backBuffer.desc.extent = {swapchainExtent_.width, swapchainExtent_.height, 1};
        backBuffer.desc.format = swapchainFormat_;
        backBuffer.desc.usage = rhi::RHITextureUsage::ColorAttachment |
                                rhi::RHITextureUsage::Present;
        backBuffer.desc.initialState = rhi::RHIResourceState::Present;
        packet.graph.textures.push_back(backBuffer);

        rhi::RHIRenderGraphTextureDesc depth{};
        depth.name = "Depth";
        depth.imported = true;
        depth.flags = rhi::RHIRenderGraphResourceFlags::Imported;
        depth.externalHandle = depthTexture_;
        depth.desc.extent = {swapchainExtent_.width, swapchainExtent_.height, 1};
        depth.desc.format = rhi::RHIFormat::D32_Float;
        depth.desc.usage = rhi::RHITextureUsage::DepthStencilAttachment;
        packet.graph.textures.push_back(depth);

        // shadowTexture_ 在图外长期创建，所以作为 Imported 资源交给 RenderGraph。
        // RenderGraph 不拥有它的生命周期，但会追踪本帧内的状态和访问顺序。
        // 同一资源同时声明 DepthStencilAttachment 与 Sampled，正好对应先写后读两种用途。
        rhi::RHIRenderGraphTextureDesc shadowDepth{};
        shadowDepth.name = "ShadowDepth";
        shadowDepth.imported = true;
        shadowDepth.flags = rhi::RHIRenderGraphResourceFlags::Imported;
        shadowDepth.externalHandle = shadowTexture_;
        shadowDepth.desc.extent = {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 1};
        shadowDepth.desc.format = rhi::RHIFormat::D32_Float;
        shadowDepth.desc.usage = rhi::RHITextureUsage::DepthStencilAttachment |
                                 rhi::RHITextureUsage::Sampled;
        packet.graph.textures.push_back(shadowDepth);

        // Cubemap 本帧仅在 Fragment Shader 中读取；首帧上传由 packet.uploads 先行完成。
        rhi::RHIRenderGraphTextureDesc skybox{};
        skybox.name = "SkyboxCube";
        skybox.imported = true;
        skybox.flags = rhi::RHIRenderGraphResourceFlags::Imported;
        skybox.externalHandle = skyboxTexture_;
        skybox.desc.dimension = rhi::RHITextureDimension::Texture2D;
        skybox.desc.extent = {skyboxWidth_, skyboxHeight_, 1};
        skybox.desc.arrayLayers = 6;
        skybox.desc.format = rhi::RHIFormat::RGBA8_SRGB;
        skybox.desc.usage = rhi::RHITextureUsage::Sampled |
                            rhi::RHITextureUsage::TransferDestination;
        skybox.desc.flags = rhi::RHITextureCreateFlags::CubeCompatible;
        packet.graph.textures.push_back(skybox);

        const auto importMaterialTexture = [&](std::size_t index) {
            const MaterialTexture& material = materialTextures_[index];
            const MaterialTextureSpec& spec = MATERIAL_TEXTURE_SPECS[index];
            rhi::RHIRenderGraphTextureDesc resource{};
            resource.name = spec.graphName;
            resource.imported = true;
            resource.flags = rhi::RHIRenderGraphResourceFlags::Imported;
            resource.externalHandle = material.texture;
            resource.desc.dimension = rhi::RHITextureDimension::Texture2D;
            resource.desc.extent = {material.width, material.height, 1};
            resource.desc.format = spec.format;
            resource.desc.usage = rhi::RHITextureUsage::Sampled |
                                  rhi::RHITextureUsage::TransferDestination;
            packet.graph.textures.push_back(std::move(resource));
        };
        for (std::size_t index = 0; index < MATERIAL_TEXTURE_COUNT; ++index) {
            importMaterialTexture(index);
        }
        if (ui_ != nullptr) {
            ui_->ImportResources(packet.graph);
        }

        // -----------------------------------------------------------------
        // Pass 1：从光源视角生成 Shadow Map。
        // -----------------------------------------------------------------
        // reads 声明顶点、索引和球体 UBO 的读取阶段；depth attachment 会被编译器
        // 自动视为对 ShadowDepth 的 DepthWrite。这里只画 sphere，所以 sphere 是 caster。
        rhi::RHIRenderGraphPassDesc shadowPass{};
        shadowPass.name = "ShadowMap";
        shadowPass.type = rhi::RHIRenderGraphPassType::Raster;
        shadowPass.reads = {
            {"Vertices", rhi::RHIRenderGraphResourceType::Buffer, rhi::RHIResourceState::VertexBuffer, rhi::RHIPipelineStage::VertexInput},
            {"Indices", rhi::RHIRenderGraphResourceType::Buffer, rhi::RHIResourceState::IndexBuffer, rhi::RHIPipelineStage::VertexInput},
            {"SphereUniform", rhi::RHIRenderGraphResourceType::Buffer, rhi::RHIResourceState::ConstantBuffer, rhi::RHIPipelineStage::VertexShader}};

        rhi::RHIRenderGraphAttachmentDesc shadowAttachment{};
        shadowAttachment.resourceName = "ShadowDepth";
        shadowAttachment.aspect = rhi::RHITextureAspect::Depth;
        // 每帧都从 1.0（最远深度）开始。Clear 避免保留上一帧球体位置造成残影。
        shadowAttachment.loadOp = rhi::RHILoadOp::Clear;
        // 后续 OpaquePBR 要采样结果，必须 Store；Discard 会允许后端丢掉深度内容。
        shadowAttachment.storeOp = rhi::RHIStoreOp::Store;
        shadowAttachment.clearValue.depthStencil = {1.0F, 0};
        shadowPass.depthStencilAttachment = shadowAttachment;
        forwardPipeline.SetPass(
            rhi::pipeline::ForwardStage::Shadow,
            std::move(shadowPass));

        // -----------------------------------------------------------------
        // Pass 2：主相机 PBR 绘制。
        // -----------------------------------------------------------------
        // 对 ShadowDepth 声明 FragmentShader/ShaderRead 后，RenderGraph 能从同一资源的
        // DepthWrite -> ShaderRead 自动推导：
        // - ShadowMap 必须先于 OpaquePBR；
        // - Vulkan 插入 image layout/access barrier；
        // - D3D12 插入 DEPTH_WRITE -> PIXEL_SHADER_RESOURCE transition；
        // - D3D11 虽无显式 barrier，也会按编译后的 Pass 顺序解绑 DSV 再绑定 SRV。
        rhi::RHIRenderGraphPassDesc opaque{};
        opaque.name = "OpaquePBR";
        opaque.type = rhi::RHIRenderGraphPassType::Raster;
        opaque.reads = {
            {"Vertices", rhi::RHIRenderGraphResourceType::Buffer, rhi::RHIResourceState::VertexBuffer, rhi::RHIPipelineStage::VertexInput},
            {"Indices", rhi::RHIRenderGraphResourceType::Buffer, rhi::RHIResourceState::IndexBuffer, rhi::RHIPipelineStage::VertexInput},
            {"SphereUniform", rhi::RHIRenderGraphResourceType::Buffer, rhi::RHIResourceState::ConstantBuffer, rhi::RHIPipelineStage::VertexShader | rhi::RHIPipelineStage::FragmentShader},
            {"PlaneUniform", rhi::RHIRenderGraphResourceType::Buffer, rhi::RHIResourceState::ConstantBuffer, rhi::RHIPipelineStage::VertexShader | rhi::RHIPipelineStage::FragmentShader},
            {"ShadowDepth", rhi::RHIRenderGraphResourceType::Texture, rhi::RHIResourceState::ShaderRead, rhi::RHIPipelineStage::FragmentShader},
            {"SkyboxCube", rhi::RHIRenderGraphResourceType::Texture, rhi::RHIResourceState::ShaderRead, rhi::RHIPipelineStage::FragmentShader},
            {"MetalBaseColor", rhi::RHIRenderGraphResourceType::Texture, rhi::RHIResourceState::ShaderRead, rhi::RHIPipelineStage::FragmentShader},
            {"MetalNormal", rhi::RHIRenderGraphResourceType::Texture, rhi::RHIResourceState::ShaderRead, rhi::RHIPipelineStage::FragmentShader},
            {"MetalMetallic", rhi::RHIRenderGraphResourceType::Texture, rhi::RHIResourceState::ShaderRead, rhi::RHIPipelineStage::FragmentShader},
            {"MetalRoughness", rhi::RHIRenderGraphResourceType::Texture, rhi::RHIResourceState::ShaderRead, rhi::RHIPipelineStage::FragmentShader},
            {"MetalHeight", rhi::RHIRenderGraphResourceType::Texture, rhi::RHIResourceState::ShaderRead, rhi::RHIPipelineStage::FragmentShader}};

        rhi::RHIRenderGraphAttachmentDesc colorAttachment{};
        colorAttachment.resourceName = "BackBuffer";
        colorAttachment.loadOp = rhi::RHILoadOp::Clear;
        colorAttachment.storeOp = rhi::RHIStoreOp::Store;
        colorAttachment.clearValue.color = {0.02F, 0.02F, 0.02F, 1.0F};
        opaque.colorAttachments.push_back(colorAttachment);

        rhi::RHIRenderGraphAttachmentDesc depthAttachment{};
        depthAttachment.resourceName = "Depth";
        depthAttachment.aspect = rhi::RHITextureAspect::Depth;
        depthAttachment.loadOp = rhi::RHILoadOp::Clear;
        depthAttachment.storeOp = rhi::RHIStoreOp::Store;
        depthAttachment.clearValue.depthStencil = {1.0F, 0};
        opaque.depthStencilAttachment = depthAttachment;
        forwardPipeline.SetPass(
            rhi::pipeline::ForwardStage::Opaque,
            std::move(opaque));

        // Loading the scene attachments makes UI the final visual pass without clearing PBR.
        if (ui_ != nullptr) {
            rhi::RHIRenderGraphPassDesc uiPass{};
            uiPass.name = "UI";
            uiPass.type = rhi::RHIRenderGraphPassType::Raster;
            ui_->AppendPassReads(uiPass);

            rhi::RHIRenderGraphAttachmentDesc uiColorAttachment{};
            uiColorAttachment.resourceName = "BackBuffer";
            uiColorAttachment.loadOp = rhi::RHILoadOp::Load;
            uiColorAttachment.storeOp = rhi::RHIStoreOp::Store;
            uiPass.colorAttachments.push_back(uiColorAttachment);

            rhi::RHIRenderGraphAttachmentDesc uiDepthAttachment{};
            uiDepthAttachment.resourceName = "Depth";
            uiDepthAttachment.aspect = rhi::RHITextureAspect::Depth;
            uiDepthAttachment.loadOp = rhi::RHILoadOp::Load;
            uiDepthAttachment.storeOp = rhi::RHIStoreOp::Store;
            uiPass.depthStencilAttachment = uiDepthAttachment;
            forwardPipeline.SetPass(
                rhi::pipeline::ForwardStage::Overlay,
                std::move(uiPass));
        }

        // Present Pass 把 BackBuffer 从 color attachment 状态转换回呈现状态。
        rhi::RHIRenderGraphPassDesc presentPass{};
        presentPass.name = "Present";
        presentPass.type = rhi::RHIRenderGraphPassType::Present;
        presentPass.queue = rhi::RHIQueueType::Present;
        presentPass.reads.push_back({
            "BackBuffer",
            rhi::RHIRenderGraphResourceType::SwapchainImage,
            rhi::RHIResourceState::Present,
            rhi::RHIPipelineStage::BottomOfPipe});
        forwardPipeline.SetPass(
            rhi::pipeline::ForwardStage::Present,
            std::move(presentPass));

        // PassDesc 只描述依赖和 attachment；Workload 才保存真正的 draw command。
        // Shadow Pass 必须使用 Shadow Map 自己的 2048x2048 viewport/scissor，不能沿用窗口
        // 尺寸，否则只会写入深度图的一部分，或者产生错误的 texel 到像素映射。
        rhi::RHIRenderPassWorkload shadowWorkload{};
        shadowWorkload.passName = "ShadowMap";
        shadowWorkload.viewport = {
            0.0F,
            0.0F,
            static_cast<float>(SHADOW_MAP_SIZE),
            static_cast<float>(SHADOW_MAP_SIZE),
            0.0F,
            1.0F};
        shadowWorkload.scissor = {{0, 0}, {SHADOW_MAP_SIZE, SHADOW_MAP_SIZE}};

        // Shadow draw 复用主场景的 vertex/index buffer，但切换为 depth-only Pipeline 和
        // 只含 UBO 的 BindSet。索引范围只覆盖球体，Plane 不作为 caster 绘制。
        rhi::RHIDrawIndexedCommand shadowSphereDraw{};
        shadowSphereDraw.pipeline = shadowPipeline_;
        shadowSphereDraw.bindSets = {shadowSphereBindSet_};
        shadowSphereDraw.vertexStreams = {{vertexBuffer_, 0, 0, sizeof(Vertex)}};
        shadowSphereDraw.indexStream.buffer = indexBuffer_;
        shadowSphereDraw.indexStream.indexType = rhi::RHIIndexType::UInt32;
        shadowSphereDraw.indexStream.offset = sphereIndexOffset_;
        shadowSphereDraw.indexStream.indexCount = sphereIndexCount_;
        shadowSphereDraw.indexCount = sphereIndexCount_;
        rhi::renderer::RHIIndexedDrawItem shadowItem{};
        shadowItem.command = std::move(shadowSphereDraw);
        shadowItem.sortKey = {0, shadowSphereBindSet_.value, indexBuffer_.value, 0.0F};
        rhi::renderer::RHIPreparedIndexedDraws preparedShadowDraws =
            rhi::renderer::PrepareOpaqueIndexedDraws({shadowItem});
        shadowWorkload.indexedDraws = std::move(preparedShadowDraws.draws);
        forwardPipeline.SetWorkload(
            rhi::pipeline::ForwardStage::Shadow,
            std::move(shadowWorkload));

        // 主 workload 依次绘制球、地面和背景。Skybox 不写深度，因此放在最后不会覆盖物体。
        rhi::RHIRenderPassWorkload opaqueWorkload{};
        opaqueWorkload.passName = "OpaquePBR";
        opaqueWorkload.viewport = packet.settings.viewport;
        opaqueWorkload.scissor = packet.settings.scissor;

        rhi::RHIDrawIndexedCommand sphereDraw{};
        sphereDraw.pipeline = pipeline_;
        sphereDraw.bindSets = {sphereBindSet_};
        sphereDraw.vertexStreams = {{vertexBuffer_, 0, 0, sizeof(Vertex)}};
        sphereDraw.indexStream.buffer = indexBuffer_;
        sphereDraw.indexStream.indexType = rhi::RHIIndexType::UInt32;
        sphereDraw.indexStream.offset = sphereIndexOffset_;
        sphereDraw.indexStream.indexCount = sphereIndexCount_;
        sphereDraw.indexCount = sphereIndexCount_;
        opaqueWorkload.indexedDraws.push_back(sphereDraw);

        rhi::RHIDrawIndexedCommand planeDraw{};
        planeDraw.pipeline = pipeline_;
        planeDraw.bindSets = {planeBindSet_};
        planeDraw.vertexStreams = {{vertexBuffer_, 0, 0, sizeof(Vertex)}};
        planeDraw.indexStream.buffer = indexBuffer_;
        planeDraw.indexStream.indexType = rhi::RHIIndexType::UInt32;
        planeDraw.indexStream.offset = planeIndexOffset_;
        planeDraw.indexStream.indexCount = planeIndexCount_;
        planeDraw.indexCount = planeIndexCount_;
        planeDraw.vertexOffsetElements = static_cast<rhi::i32>(sphereVertexCount_);
        opaqueWorkload.indexedDraws.push_back(planeDraw);

        rhi::RHIDrawIndexedCommand skyboxDraw{};
        skyboxDraw.pipeline = skyboxPipeline_;
        skyboxDraw.bindSets = {skyboxBindSet_};
        skyboxDraw.vertexStreams = {{vertexBuffer_, 0, 0, sizeof(Vertex)}};
        skyboxDraw.indexStream.buffer = indexBuffer_;
        skyboxDraw.indexStream.indexType = rhi::RHIIndexType::UInt32;
        skyboxDraw.indexStream.offset = sphereIndexOffset_;
        skyboxDraw.indexStream.indexCount = sphereIndexCount_;
        skyboxDraw.indexCount = sphereIndexCount_;
        std::vector<rhi::renderer::RHIIndexedDrawItem> opaqueItems;
        opaqueItems.reserve(3);

        rhi::renderer::RHIIndexedDrawItem sphereItem{};
        sphereItem.command = std::move(sphereDraw);
        sphereItem.sortKey = {0, sphereBindSet_.value, indexBuffer_.value, 0.0F};
        opaqueItems.push_back(std::move(sphereItem));

        rhi::renderer::RHIIndexedDrawItem planeItem{};
        planeItem.command = std::move(planeDraw);
        planeItem.sortKey = {0, planeBindSet_.value, indexBuffer_.value, 0.0F};
        opaqueItems.push_back(std::move(planeItem));

        rhi::renderer::RHIIndexedDrawItem skyboxItem{};
        skyboxItem.command = std::move(skyboxDraw);
        // Skybox belongs to a later render layer so sorting cannot move it ahead of scene geometry.
        skyboxItem.sortKey = {1, skyboxBindSet_.value, indexBuffer_.value, 0.0F};
        opaqueItems.push_back(std::move(skyboxItem));

        rhi::renderer::RHIPreparedIndexedDraws preparedOpaqueDraws =
            rhi::renderer::PrepareOpaqueIndexedDraws(opaqueItems);
        opaqueWorkload.indexedDraws = std::move(preparedOpaqueDraws.draws);
        forwardPipeline.SetWorkload(
            rhi::pipeline::ForwardStage::Opaque,
            std::move(opaqueWorkload));

        if (ui_ != nullptr) {
            rhi::RHIRenderPassWorkload uiWorkload{};
            uiWorkload.passName = "UI";
            uiWorkload.viewport = packet.settings.viewport;
            uiWorkload.scissor = packet.settings.scissor;
            ui_->AppendDraws(uiWorkload);
            forwardPipeline.SetWorkload(
                rhi::pipeline::ForwardStage::Overlay,
                std::move(uiWorkload));
        }

        // 所有阶段准备完后再写入 FramePacket。这里不做资源状态推导；SubmitFrame
        // 随后调用的 RenderGraph 编译器仍是依赖、barrier 与 transient alias 的唯一来源。
        forwardPipeline.Commit(packet);

        // 三个 Pass 放在同一次 Graphics Queue submission 中。passNames 的顺序还会接受
        // RenderGraph 依赖验证，防止调用方把消费者 OpaquePBR 提交到生产者 ShadowMap 前面。
        rhi::RHIQueueSubmitDesc submit{};
        submit.debugName = "PBR.RenderGraphSubmit";
        submit.queue = rhi::RHIQueueType::Graphics;
        submit.passNames = forwardPipeline.PassNames();
        submit.waits.push_back({
            imageAvailable_[frameSlot_],
            0,
            rhi::RHIPipelineStage::ColorAttachmentOutput});
        submit.signals.push_back({renderFinished_[frameSlot_], 0});
        packet.submissions.push_back(submit);

        // acquire signal 保证 backbuffer 可写，render-finished signal 保证 present 只读取完成帧。
        packet.present = rhi::RHIPresentDesc{
            swapchain_,
            imageIndex,
            {renderFinished_[frameSlot_]},
            rhi::RHIPresentMode::FIFO,
            false};
        return packet;
    }

    /// 获取一张 swapchain image，提交 RenderGraph 帧，并推进同步槽位和累计帧号。
    void DrawFrame() {
        // 一帧的 CPU 顺序是 acquire -> BuildFrame/上传 -> SubmitFrame -> present。
        // acquire 失败通常意味着窗口尺寸变化，交给下一轮 RecreateSwapchain 处理。
        if (!swapchain_) {
            return;
        }
#if defined(PBRDEMO_ENABLE_RENDERDOC_CAPTURE)
        const bool captureThisFrame = renderDocCapturePending_ && renderDocApi_ != nullptr;
        if (captureThisFrame) {
            // nullptr device uses RenderDoc's active API target; HWND narrows the capture to
            // this demo window. Start before acquire so the complete present path is recorded.
            renderDocApi_->StartFrameCapture(nullptr, window_);
            renderDocApi_->SetCaptureTitle(DEMO_CAPTURE_TITLE);
        }
#endif
        rhi::u32 imageIndex = 0;
        std::string error;
        if (!device_->AcquireNextImage(
                swapchain_,
                imageAvailable_[frameSlot_],
                {},
                &imageIndex,
                &error)) {
#if defined(PBRDEMO_ENABLE_RENDERDOC_CAPTURE)
            if (captureThisFrame) {
                renderDocApi_->DiscardFrameCapture(nullptr, window_);
            }
#endif
            framebufferResized_ = true;
            return;
        }

        const rhi::RHIFramePacket packet = BuildFrame(imageIndex);
        if (!device_->SubmitFrame(packet, &error)) {
            if (framebufferResized_) {
#if defined(PBRDEMO_ENABLE_RENDERDOC_CAPTURE)
                if (captureThisFrame) {
                    renderDocApi_->DiscardFrameCapture(nullptr, window_);
                }
#endif
                return;
            }
#if defined(PBRDEMO_ENABLE_RENDERDOC_CAPTURE)
            if (captureThisFrame) {
                renderDocApi_->DiscardFrameCapture(nullptr, window_);
            }
#endif
            throw std::runtime_error("RenderGraph frame submission failed: " + error);
        }
#if defined(PBRDEMO_ENABLE_RENDERDOC_CAPTURE)
        if (captureThisFrame) {
            if (renderDocApi_->EndFrameCapture(nullptr, window_) == 0) {
                throw std::runtime_error("RenderDoc failed to save the PBR frame capture");
            }
            renderDocCapturePending_ = false;
        }
#endif
        staticUploadsPending_ = false;
        frameSlot_ = (frameSlot_ + 1) % FRAMES_IN_FLIGHT;
        ++frameIndex_;
    }

    /// 处理 Win32 消息、最小化等待、swapchain 重建和逐帧渲染。
    void MainLoop() {
        // PeekMessage 保证渲染不会被 GetMessage 阻塞；最小化时 Sleep，避免 0x0 swapchain
        // 忙等。WM_SIZE/鼠标消息只更新状态，真正的 GPU 工作统一在 DrawFrame 完成。
        MSG message{};
        while (running_) {
            while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
                if (message.message == WM_QUIT) {
                    running_ = false;
                    break;
                }
                TranslateMessage(&message);
                DispatchMessage(&message);
            }
            if (!running_) {
                break;
            }
            const rhi::RHIExtent2D extent = ClientExtent();
            if (extent.width == 0 || extent.height == 0) {
                Sleep(16);
                continue;
            }
            if (framebufferResized_) {
                RecreateSwapchain();
            }
            DrawFrame();
            if (options_.maxFrames != 0 && frameIndex_ >= options_.maxFrames) {
                running_ = false;
            }
        }
    }

    /// 等待 GPU 空闲，并按“使用者先于被引用资源”的逆依赖顺序销毁对象。
    void Cleanup() noexcept {
        // WaitIdle 后按“使用者先于被引用者”的逆序销毁，确保 descriptor、pipeline、
        // view 在底层 texture/buffer 之前释放，并让 UI 在 device 仍有效时 Shutdown。
        if (device_ == nullptr) {
            return;
        }
        device_->WaitIdle();
        if (ui_ != nullptr) {
            ui_->Shutdown();
            ui_.reset();
        }
        // 销毁顺序与引用关系相反：先销毁使用资源的 Pipeline/BindSet/Layout，再销毁
        // Sampler/View/Texture。WaitIdle 保证 GPU 不再访问这些对象。
        device_->Destroy(pipeline_);
        device_->Destroy(shadowPipeline_);
        device_->Destroy(skyboxPipeline_);
        device_->Destroy(pipelineLayout_);
        device_->Destroy(shadowPipelineLayout_);
        device_->Destroy(skyboxPipelineLayout_);
        device_->Destroy(sphereBindSet_);
        device_->Destroy(planeBindSet_);
        device_->Destroy(shadowSphereBindSet_);
        device_->Destroy(skyboxBindSet_);
        device_->Destroy(bindSetLayout_);
        device_->Destroy(shadowBindSetLayout_);
        device_->Destroy(skyboxBindSetLayout_);
        device_->Destroy(shadowSampler_);
        device_->Destroy(shadowView_);
        device_->Destroy(shadowTexture_);
        device_->Destroy(skyboxSampler_);
        device_->Destroy(skyboxView_);
        device_->Destroy(skyboxTexture_);
        device_->Destroy(materialSampler_);
        for (std::size_t index = MATERIAL_TEXTURE_COUNT; index > 0; --index) {
            MaterialTexture& material = materialTextures_[index - 1];
            device_->Destroy(material.view);
            device_->Destroy(material.texture);
        }
        device_->Destroy(sphereUniformBuffer_);
        device_->Destroy(planeUniformBuffer_);
        device_->Destroy(indexBuffer_);
        device_->Destroy(vertexBuffer_);
        device_->Destroy(depthView_);
        device_->Destroy(depthTexture_);
        device_->Destroy(swapchain_);
        for (rhi::u32 index = 0; index < FRAMES_IN_FLIGHT; ++index) {
            device_->Destroy(imageAvailable_[index]);
            device_->Destroy(renderFinished_[index]);
        }
        device_->Shutdown();
    }
};

} // namespace

/// Windows GUI 入口：运行基于 HWND 的 Demo，并将异常显示为消息框。
int APIENTRY WinMain(HINSTANCE instance, HINSTANCE, LPSTR commandLine, int) {
    try {
        PBRDemoApp app(ParseOptions(commandLine != nullptr ? commandLine : ""));
        app.Run(instance);
        return EXIT_SUCCESS;
    } catch (const std::exception& exception) {
        MessageBoxA(nullptr, exception.what(), DEMO_ERROR_TITLE, MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }
}
