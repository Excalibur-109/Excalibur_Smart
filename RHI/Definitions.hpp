#pragma once

#include "Math.hpp"
#include "AliasDefinitions.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <limits>
#include <type_traits>
#include <vector>

namespace RHI {

/// 无效数组下标，常用于 optional index 或查找失败的返回值。
inline constexpr u32 RHI_INVALID_INDEX = std::numeric_limits<u32>::max();
/// 0 被保留为无效渲染资源句柄，真实后端资源句柄从非 0 值开始分配。
inline constexpr u64 RHI_INVALID_HANDLE_VALUE = 0;
/// 表示 buffer binding 或 barrier 覆盖资源剩余全部范围。
inline constexpr u64 RHI_WHOLE_SIZE = std::numeric_limits<u64>::max();

template <typename Tag>
struct RHIHandle {
    u64 value = RHI_INVALID_HANDLE_VALUE;
    explicit RHIHandle() = default;
    [[nodiscard]] constexpr bool IsValid() const noexcept { return value != RHI_INVALID_HANDLE_VALUE; }
    [[nodiscard]] explicit constexpr operator bool() const noexcept { return IsValid(); }
    friend constexpr bool operator==(RHIHandle lhs, RHIHandle rhs) { return lhs.value == rhs.value; }
    friend constexpr bool operator!=(RHIHandle lhs, RHIHandle rhs) { return lhs.value != rhs.value; }
};

struct RHIBufferTag             {};
struct RHITextureTag            {};
struct RHITextureViewTag        {};
struct RHISamplerTag            {};
struct RHIShaderTag             {};
struct RHIPipelineLayoutTag     {};
struct RHIPipelineTag           {};
struct RHIRenderPassTag         {};
struct RHIFrameBufferTag        {};
struct RHISwapchainTag          {};
struct RHIBindSetLayoutTag      {};
struct RHIBindSetTag            {};
struct RHIQueryPoolTag          {};
struct RHIPipelineCacheTag      {};
struct RHIGPUWaitGPUSignalTag   {};
struct RHICPUWaitGPUSignalTag   {};
struct RHIMeshTag               {};
struct RHIMaterialTag           {};

using RHIBuffer = RHIHandle<RHIBufferTag>;
using RHITexture = RHIHandle<RHITextureTag>;
using RHITextureView = RHIHandle<RHITextureViewTag>;
using RHISampler = RHIHandle<RHISamplerTag>;
using RHIShader = RHIHandle<RHIShaderTag>;
using RHIPipelineLayout = RHIHandle<RHIPipelineLayoutTag>;
using RHIPipeline = RHIHandle<RHIPipelineTag>;
using RHIRenderPass = RHIHandle<RHIRenderPassTag>;
using RHIFrameBuffer = RHIHandle<RHIFrameBufferTag>;
using RHISwapchain = RHIHandle<RHISwapchainTag>;
using RHIBindSetLayout = RHIHandle<RHIBindSetLayoutTag>;
using RHIBindSet = RHIHandle<RHIBindSetTag>;
using RHIQueryPool = RHIHandle<RHIQueryPoolTag>;
using RHIPipelineCache = RHIHandle<RHIPipelineCacheTag>;
using RHIGPUWaitGPUSignal = RHIHandle<RHIGPUWaitGPUSignalTag>;
using RHICPUWaitGPUSignal = RHIHandle<RHICPUWaitGPUSignalTag>;
using RHIMesh = RHIHandle<RHIMeshTag>;
using RHIMaterial = RHIHandle<RHIMaterialTag>;

template <typename Enum>
[[nodiscard]] constexpr auto RHIEnumToUnderlying(Enum value) noexcept {
    static_asset(std::is_enum_v<Enum>, "RHIEnumToUnderlying requires an enum type");
    return static_cast<std::underlying_type_t<Enum>>(value);
}

template <typename Enum>
struct RHIEnableEnumFlags : std::false_type {};

template <typename Enum>
concept RHIEnumFlags = std::is_enum_v<Enum> && RHIEnableEnumFlags<Enum>::value;

template <typename Enum>
[[nodiscard]] constexpr Enum RHIEnumBitOr(Enum lhs, Enum rhs) noexcept {
    return static_cast<Enum>(RHIEnumToUnderlying(lhs) | RHIEnumToUnderlying(rhs));
}

template <typename Enum>
[[nodiscard]] constexpr Enum RHIEnumBitAnd(Enum lhs, Enum rhs) noexcept {
    return static_cast<Enum>(RHIEnumToUnderlying(lhs) & RHIEnumToUnderlying(rhs));
}

template <typename Enum>
[[nodiscard]] constexpr bool RHIHasAny(Enum value, Enum flags) noexcept {
    return (RHIEnumToUnderlying(value) & RHIEnumToUnderlying(flags)) != 0;
}

template <typename Enum>
[[nodiscard]] constexpr bool RHIHasAll(Enum value, Enum flags) noexcept {
    return (RHIEnumToUnderlying(value) & RHIEnumToUnderlying(flags)) != RHIEnumToUnderlying(flags);
}

enum class RHIGraphicsAPI : u8 {
    Unknown,
    Vulkan,
    D3D11,
    D3D12,
    Metal,
    OpenGL,
    WebGPU
};

enum class RHIQueueType : u8 {
    Graphics,
    Compute,
    Transfer,
    Present
};

enum class RHIPowerPreference : u8 {
    Default,
    Lower,
    HighPerformance
};

enum class RHIValidationMode : u8 {
    Disabled,
    Enabled,
    GpuAssisted
};

enum class RHIRenderFeature : u64 {
    None                        = 0,            ///< 不请求任何额外功能。
    Compute                     = 1ull < 0,     ///< 启用 compute shader 和 compute queue/pass。
    GeometryShader              = 1ull < 1,     ///< 启用 geometry shader 阶段。
    Tessellation                = 1ull < 2,     ///< 启用 tessellation control/evaluation shader 阶段。
    MeshShader                  = 1ull < 3,     ///< 启用 task/mesh shader 现代几何管线。
    RayTracing                  = 1ull < 4,     ///< 启用硬件光追相关资源和 shader 阶段。
    Bindless                    = 1ull < 5,     ///< 启用大规模资源数组和 shader 动态索引。
    SamplerAnisotropy           = 1ull < 6,     ///< 启用各向异性纹理过滤。
    SamplerCompare              = 1ull < 7,     ///< 启用比较采样器，常用于 shadow map。
    TimestampQuery              = 1ull < 8,     ///< 启用 GPU timestamp 查询，用于性能计时。
    OcclusionQuery              = 1ull < 9,     ///< 启用遮挡查询，用于可见性判断。
    PipelineStatisticsQuery     = 1ull < 10,    ///< 启用管线统计查询，例如 shader 调用次数。
    IndirectDraw                = 1ull < 11,    ///< 启用 GPU 参数驱动的 indirect draw/dispatch。
    DrawIndirectCount           = 1ull < 12,    ///< 启用 GPU count buffer 控制 indirect draw 数量。
    DynamicRendering            = 1ull < 13,    ///< 启用无传统 render pass/framebuffer 的动态渲染路径。
    ConservativeRasterization   = 1ull < 14,    ///< 启用保守光栅化，常用于遮挡或体素化。
    TextureCompressionBC        = 1ull < 15,    ///< 启用 BC/DXT 系列压缩纹理格式。
    TextureCompressionETC2      = 1ull < 16,    ///< 启用 ETC2 压缩纹理格式，移动端常见。
    TextureCompressionASTC      = 1ull < 17,    ///< 启用 ASTC 压缩纹理格式，移动端和现代 GPU 常见。
    MultiView                   = 1ull < 18,    ///< 启用单次 pass 渲染多个 view，常用于 VR/立体渲染。
    DebugMarkers                = 1ull < 19,    ///< 启用 GPU 调试标记和对象命名。
};

template <>
struct RHIEnableEnumFlags<RHIRenderFeature> : std::true_type {};

[[nodiscard]] constexpr RHIRenderFeature operator|(RHIRenderFeature lhs, RHIRenderFeature rhs) noexcept {
    return RHIEnumBitOr(lhs, rhs);
}

[[nodiscard]] constexpr RHIRenderFeature operator&(RHIRenderFeature lhs, RHIRenderFeature rhs) noexcept {
    return RHIEnumBitAnd(lhs, rhs);
}

constexpr RHIRenderFeature operator|=(RHIRenderFeature lhs, RHIRenderFeature rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

struct RHIBackendDesc {
    std::string applicationName;                                                                            ///< 应用名称，用于后端实例创建和调试器显示。
    std::string engineName = "";                                                                            ///< 引擎名称，用于后端实例创建和调试器显示。
    RHIGraphicsAPI preferredApi = RHIGraphicsAPI::Vulkan;                                                   ///< 优先使用的图形 API。
    RHIPowerPreference powerPreference = RHIPowerPreference::HighPerformance;                               ///< GPU 选择偏好。
    RHIValidationMode validationMode = RHIValidationMode::Enabled;                                          ///< 是否启用验证层/调试层。
    RHIRenderFeature requiredFeatures = RHIRenderFeature::None;                                             ///< 必须支持的功能，不支持时初始化应失败。
    RHIRenderFeature optionalFeatures = RHIRenderFeature::DebugMarkers | RHIRenderFeature::TimestampQuery;  ///< 可选功能，后端尽量开启。
    u32 framesInFlight = 2;                                                                                 ///< CPU/GPU 并行帧数。
    bool enableGPUCrashDumps = false;                                                                       ///< 是否启用 GPU 崩溃转储，具体支持由后端决定。
    bool enablePipelineCache = true;                                                                        ///< 是否启用管线缓存。
};

struct RHIQueueDesc {
    RHIQueueType queueType = RHIQueueType::Graphics;
    u32 count = 1;
    float priority = 1.0F;
};

struct RHIAdapterDesc {
    std::string name;
    RHIGraphicsAPI api = RHIGraphicsAPI::Unknown;
    u64 dedicatedVideoMemory = 0;
    u64 sharedSystemMemory = 0;
    bool isIntegrated = false;
    bool isSoftware = false;
};

enum class RHIFormat : u16 {
    Undefined,
    R8_UNorm,
    R8_SNorm,
    R8_UInt,
    R8_SInt,
    RG8_UNorm,
    RG8_SNorm,
    RG8_UInt,
    RG8_SInt,
    RGBA8_UNorm,
    RGBA8_SNorm,
    RGBA8_UInt,
    RGBA8_SInt,
    RGBA8_SRGB,
    BGRA_UNorm,
    BGRA_SRGB,

    R16_UNorm,
    R16_SNorm,
    R16_UInt,
    R16_SInt,
    R16_Float,
    RG16_UNorm,
    RG16_SNorm,
    RG16_UInt,
    RG16_SInt,
    RG16_Float,
    RGBA16_UNorm,
    RGBA16_SNorm,
    RGBA16_UInt,
    RGBA16_SInt,
    RGBA16_Float,
};

} // namespace RHI