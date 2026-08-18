#pragma once

#include "RHI/Device/RHIDevice.hpp"

#include <string>
#include <string_view>
#include <vector>

// UI 只依赖 Device 的公开 RHI 接口：它不访问 Vulkan、D3D11 或 D3D12 的
// 原生对象，因此可以作为一个普通的上层模块编译。Context 采用“每帧提交
// 控件、随后一次性录制 draw”的即时模式，适合 Demo 和工具型界面。
namespace RHI::UI {

struct Color {
    float r = 1.0F; // 线性/归一化红色分量，范围通常为 [0, 1]。
    float g = 1.0F; // 绿色分量。
    float b = 1.0F; // 蓝色分量。
    float a = 1.0F; // 不透明度；管线用它参与 source-alpha 混合。
};

struct Rect {
    float x = 0.0F;      // 相对于客户区左上角的像素坐标。
    float y = 0.0F;
    float width = 0.0F;  // 控件的逻辑宽度；提交时转换为 0~1 的屏幕坐标。
    float height = 0.0F;

    [[nodiscard]] bool Contains(float pointX, float pointY) const noexcept {
        // UI 输入和绘制使用同一套客户区像素坐标，命中测试无需知道 GPU 的 NDC。
        return pointX >= x && pointX <= x + width &&
               pointY >= y && pointY <= y + height;
    }
};

struct InputState {
    float mouseX = 0.0F; // 当前鼠标位置（客户区像素）。
    float mouseY = 0.0F;
    bool leftButtonDown = false;     // 左键当前是否保持按下。
    bool leftButtonPressed = false;  // 本帧从抬起变为按下的边沿事件。
    bool leftButtonReleased = false; // 本帧从按下变为抬起的边沿事件。
};

/// Immediate-mode UI renderer built entirely on top of the public RHI facade.
/// Layers are painter-order values: larger layers and later submissions cover earlier ones.
class Context {
public:
    Context(
        RHIDevice& device,
        RHIFormat colorFormat,
        RHIFormat depthFormat,
        std::string shaderDirectory);
    ~Context();

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    Context(Context&&) = delete;
    Context& operator=(Context&&) = delete;

    // BeginFrame 会清空上一帧的几何和控件 ID；调用者应在本帧所有控件前调用它。
    void BeginFrame(RHIExtent2D extent, InputState input);
    // Panel 使用内部 1x1 白纹理，因此颜色完全由顶点色控制，不需要额外图片。
    void Panel(Rect rect, Color color, i32 layer = 0);
    // Text 使用内置 5x7 点阵字体，把每个亮点展开成一个小四边形。
    void Text(std::string_view value, float x, float y, float pixelHeight, Color color, i32 layer = 0);
    // TextBox 是一个带标题文字的组合控件；它目前是显示型控件，并不处理键盘输入。
    void TextBox(Rect rect, std::string_view value, Color background, Color text, i32 layer = 0);
    void Image(
        Rect rect,
        RHITexture texture,
        RHITextureView view,
        std::string_view graphResourceName,
        i32 layer = 0);
    // Button 返回“本帧按下且鼠标命中”的瞬时事件，而不是持久的 checked 状态。
    [[nodiscard]] bool Button(std::string_view label, Rect rect, i32 layer = 0);
    // SliderFloat 在左键拖动期间连续修改 value，并按 step 进行量化。
    [[nodiscard]] bool SliderFloat(
        std::string_view label,
        Rect rect,
        float& value,
        float minimum,
        float maximum,
        float step,
        i32 layer = 0);

    // 把 CPU 顶点和一次性的白纹理数据附加到当前帧上传列表。
    void AppendUploads(RHIFramePacket& frame);
    // 将 UI 自己创建的资源注册为 RenderGraph imported resource。
    void ImportResources(RHIRenderGraphDesc& graph) const;
    // 声明 UI pass 对顶点缓冲、白纹理和外部预览纹理的读状态。
    void AppendPassReads(RHIRenderGraphPassDesc& pass) const;
    // 按 layer/order 排序并生成实际的 RHI draw 命令。
    void AppendDraws(RHIRenderPassWorkload& workload);
    void Shutdown() noexcept;

private:
    struct Vertex;
    struct DrawItem;
    struct ImageBinding;

    [[nodiscard]] RHIBindSet BindSetForImage(RHITexture texture, RHITextureView view);
    void AddQuad(Rect rect, Color color, RHIBindSet bindSet, i32 layer);
    void AddTextVertices(std::string_view value, float x, float y, float pixelHeight, Color color, i32 layer);
    void AddDrawItem(RHIBindSet bindSet, i32 layer, u32 firstVertex);

    RHIDevice* device_ = nullptr;
    RHIFormat colorFormat_ = RHIFormat::Undefined;
    RHIFormat depthFormat_ = RHIFormat::Undefined;
    std::string shaderDirectory_;
    RHIExtent2D extent_{};
    InputState input_{};
    u32 activeSlider_ = RHI_INVALID_INDEX;
    u32 nextWidgetId_ = 0;
    u32 nextOrder_ = 0;
    bool staticUploadsPending_ = true;

    RHIBuffer vertexBuffer_{};
    RHITexture whiteTexture_{};
    RHITextureView whiteView_{};
    RHISampler sampler_{};
    RHIBindSetLayout bindSetLayout_{};
    RHIBindSet whiteBindSet_{};
    RHIPipelineLayout pipelineLayout_{};
    RHIPipeline pipeline_{};

    std::vector<Vertex> vertices_;
    std::vector<DrawItem> draws_;
    std::vector<ImageBinding> imageBindings_;
    std::vector<std::string> externalTextureNames_;
};

} // namespace RHI::UI
