#pragma once

#include "RHI/Device/RHIDevice.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace RHI::UI {

struct Color {
    float r = 1.0F;
    float g = 1.0F;
    float b = 1.0F;
    float a = 1.0F;
};

struct Rect {
    float x = 0.0F;
    float y = 0.0F;
    float width = 0.0F;
    float height = 0.0F;

    [[nodiscard]] bool Contains(float pointX, float pointY) const noexcept {
        return pointX >= x && pointX <= x + width &&
               pointY >= y && pointY <= y + height;
    }
};

struct InputState {
    float mouseX = 0.0F;
    float mouseY = 0.0F;
    bool leftButtonDown = false;
    bool leftButtonPressed = false;
    bool leftButtonReleased = false;
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

    void BeginFrame(RHIExtent2D extent, InputState input);
    void Panel(Rect rect, Color color, i32 layer = 0);
    void Text(std::string_view value, float x, float y, float pixelHeight, Color color, i32 layer = 0);
    void TextBox(Rect rect, std::string_view value, Color background, Color text, i32 layer = 0);
    void Image(
        Rect rect,
        RHITexture texture,
        RHITextureView view,
        std::string_view graphResourceName,
        i32 layer = 0);
    [[nodiscard]] bool Button(std::string_view label, Rect rect, i32 layer = 0);
    [[nodiscard]] bool SliderFloat(
        std::string_view label,
        Rect rect,
        float& value,
        float minimum,
        float maximum,
        float step,
        i32 layer = 0);

    void AppendUploads(RHIFramePacket& frame);
    void ImportResources(RHIRenderGraphDesc& graph) const;
    void AppendPassReads(RHIRenderGraphPassDesc& pass) const;
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
