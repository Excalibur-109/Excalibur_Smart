#include "RHIUI.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace RHI::UI {
namespace {

// 这是单帧 UI 的硬上限。每个矩形由两个三角形（6 个顶点）组成，
// 预先固定容量可以避免录制过程中不断重新分配 GPU buffer；超出时直接报错，
// 让调用者知道需要拆分界面或提高容量，而不是静默丢失控件。
constexpr u32 MAX_UI_VERTICES = 65536;

// 点阵字体只保存每行 5 个像素的 bit mask。它不依赖操作系统字体栅格化，
// 因而 Vulkan、D3D11、D3D12 的截图都能得到一致的布局；未收录字符显示为 '?'。
struct Glyph {
    char character;
    std::array<u8, 7> rows;
};

constexpr std::array GLYPHS{
    Glyph{' ', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    Glyph{'-', {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}},
    Glyph{'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}},
    Glyph{'/', {0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00}},
    Glyph{':', {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00}},
    Glyph{'?', {0x0E, 0x11, 0x02, 0x04, 0x04, 0x00, 0x04}},
    Glyph{'0', {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}},
    Glyph{'1', {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    Glyph{'2', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}},
    Glyph{'3', {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E}},
    Glyph{'4', {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}},
    Glyph{'5', {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}},
    Glyph{'6', {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}},
    Glyph{'7', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
    Glyph{'8', {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}},
    Glyph{'9', {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}},
    Glyph{'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    Glyph{'B', {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}},
    Glyph{'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}},
    Glyph{'D', {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}},
    Glyph{'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
    Glyph{'F', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}},
    Glyph{'G', {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}},
    Glyph{'H', {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    Glyph{'I', {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    Glyph{'J', {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E}},
    Glyph{'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}},
    Glyph{'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
    Glyph{'M', {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}},
    Glyph{'N', {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}},
    Glyph{'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    Glyph{'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
    Glyph{'Q', {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}},
    Glyph{'R', {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}},
    Glyph{'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}},
    Glyph{'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    Glyph{'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    Glyph{'V', {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}},
    Glyph{'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A}},
    Glyph{'X', {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}},
    Glyph{'Y', {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}},
    Glyph{'Z', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}},
};

[[nodiscard]] const Glyph& FindGlyph(char character) noexcept {
    for (const Glyph& glyph : GLYPHS) {
        if (glyph.character == character) {
            return glyph;
        }
    }
    return GLYPHS[5];
}

[[nodiscard]] float TextWidth(std::string_view value, float pixelHeight) noexcept {
    // 5 列字形加 1 列字间距，7 行高度按 pixelHeight 缩放。
    return static_cast<float>(value.size()) * pixelHeight * (6.0F / 7.0F);
}

} // namespace

struct Context::Vertex {
    float2 position{}; // 归一化客户区坐标；shader 再转换到 NDC。
    float2 uv{};       // 白纹理固定为全覆盖，图片则使用完整 [0,1] 区间。
    float4 color{};    // 顶点色与采样颜色相乘，再交给 alpha blending。
};

struct Context::DrawItem {
    RHIBindSet bindSet{}; // 每张图片的纹理+sampler 绑定；纯色控件使用 whiteBindSet_。
    i32 layer = 0;        // 数值越大越晚绘制，覆盖前面的像素。
    u32 order = 0;        // 同层保持提交顺序，保证即时模式的确定性。
    u32 firstVertex = 0;  // 在本帧线性顶点缓冲中的起始位置。
    u32 vertexCount = 0;
};

struct Context::ImageBinding {
    RHITexture texture{};     // 用于缓存键，防止同一纹理重复创建 bind set。
    RHITextureView view{};
    RHIBindSet bindSet{};     // 与 UI layout 兼容的 combined texture/sampler。
};

Context::Context(
    RHIDevice& device,
    RHIFormat colorFormat,
    RHIFormat depthFormat,
    std::string shaderDirectory)
    : device_(&device),
      colorFormat_(colorFormat),
      depthFormat_(depthFormat),
      shaderDirectory_(std::move(shaderDirectory)) {
    // 下面只创建一次、可跨帧复用的 GPU 对象。每帧变化的矩形和文字只上传顶点，
    // 这样 UI 不需要修改 RHI 核心的资源生命周期或命令录制实现。
    RHIBufferDesc vertexBufferDesc{};
    vertexBufferDesc.debugName = "UI.Vertices";
    vertexBufferDesc.size = sizeof(Vertex) * MAX_UI_VERTICES;
    vertexBufferDesc.usage = RHIBufferUsage::Vertex | RHIBufferUsage::TransferDestination;
    vertexBuffer_ = device_->CreateBuffer(vertexBufferDesc);

    RHITextureDesc whiteTextureDesc{};
    whiteTextureDesc.debugName = "UI.White";
    whiteTextureDesc.extent = {1, 1, 1};
    whiteTextureDesc.format = RHIFormat::RGBA8_UNorm;
    whiteTextureDesc.usage = RHITextureUsage::Sampled | RHITextureUsage::TransferDestination;
    whiteTexture_ = device_->CreateTexture(whiteTextureDesc);

    RHITextureViewDesc whiteViewDesc{};
    whiteViewDesc.debugName = "UI.WhiteView";
    whiteViewDesc.texture = whiteTexture_;
    whiteViewDesc.format = whiteTextureDesc.format;
    whiteView_ = device_->CreateTextureView(whiteViewDesc);

    RHISamplerDesc samplerDesc{};
    samplerDesc.debugName = "UI.Sampler";
    samplerDesc.minFilter = RHIFilterMode::Linear;
    samplerDesc.magFilter = RHIFilterMode::Linear;
    samplerDesc.mipmapMode = RHIMipmapMode::Nearest;
    samplerDesc.addressU = RHIAddressMode::ClampToEdge;
    samplerDesc.addressV = RHIAddressMode::ClampToEdge;
    samplerDesc.addressW = RHIAddressMode::ClampToEdge;
    samplerDesc.maxLod = 0.0F;
    sampler_ = device_->CreateSampler(samplerDesc);

    RHIBindSetLayoutDesc bindSetLayoutDesc{};
    bindSetLayoutDesc.debugName = "UI.BindSetLayout";
    bindSetLayoutDesc.set = 0;
    RHIBindSetLayoutEntry textureEntry{};
    textureEntry.binding = 0;
    textureEntry.type = RHIBindingType::CombinedTextureSampler;
    textureEntry.visibility = RHIShaderStage::Fragment;
    textureEntry.textureViewDimension = RHITextureViewDimension::View2D;
    textureEntry.textureSampleType = RHITextureSampleType::Float;
    bindSetLayoutDesc.entries.push_back(textureEntry);
    bindSetLayout_ = device_->CreateBindSetLayout(bindSetLayoutDesc);

    RHIBindSetDesc whiteBindSetDesc{};
    whiteBindSetDesc.debugName = "UI.WhiteBindSet";
    whiteBindSetDesc.layout = bindSetLayout_;
    RHIResourceBinding whiteBinding{};
    whiteBinding.binding = 0;
    whiteBinding.type = RHIBindingType::CombinedTextureSampler;
    whiteBinding.texture = {whiteView_, whiteTexture_};
    whiteBinding.sampler = sampler_;
    whiteBindSetDesc.bindings.push_back(whiteBinding);
    whiteBindSet_ = device_->CreateBindSet(whiteBindSetDesc);

    RHIPipelineLayoutDesc pipelineLayoutDesc{};
    pipelineLayoutDesc.debugName = "UI.PipelineLayout";
    pipelineLayoutDesc.bindSetLayouts.push_back(bindSetLayout_);
    pipelineLayout_ = device_->CreatePipelineLayout(pipelineLayoutDesc);

    RHIShaderDesc vertexShader{};
    vertexShader.debugName = "UI.VertexShader";
    vertexShader.stage = RHIShaderStage::Vertex;
    RHIShaderDesc fragmentShader{};
    fragmentShader.debugName = "UI.FragmentShader";
    fragmentShader.stage = RHIShaderStage::Fragment;
    if (device_->Api() == RHIGraphicsAPI::Vulkan) {
        vertexShader.language = RHIShaderLanguage::SPIRV;
        vertexShader.filePath = shaderDirectory_ + "/ui.vert.spv";
        fragmentShader.language = RHIShaderLanguage::SPIRV;
        fragmentShader.filePath = shaderDirectory_ + "/ui.frag.spv";
    } else {
        const bool d3d12 = device_->Api() == RHIGraphicsAPI::D3D12;
        vertexShader.language = RHIShaderLanguage::HLSL;
        vertexShader.filePath = shaderDirectory_ + "/ui.hlsl";
        vertexShader.entryPoint = "UIVS";
        vertexShader.compileOptions.targetProfile = d3d12 ? "vs_5_1" : "vs_5_0";
        fragmentShader.language = RHIShaderLanguage::HLSL;
        fragmentShader.filePath = shaderDirectory_ + "/ui.hlsl";
        fragmentShader.entryPoint = "UIPS";
        fragmentShader.compileOptions.targetProfile = d3d12 ? "ps_5_1" : "ps_5_0";
    }

    RHIVertexBufferLayoutDesc vertexLayout{};
    vertexLayout.binding = 0;
    vertexLayout.stride = sizeof(Vertex);
    vertexLayout.attributes = {
        {"POSITION", 0, 0, 0, RHIVertexFormat::Float32x2, offsetof(Vertex, position)},
        {"TEXCOORD", 0, 1, 0, RHIVertexFormat::Float32x2, offsetof(Vertex, uv)},
        {"COLOR", 0, 2, 0, RHIVertexFormat::Float32x4, offsetof(Vertex, color)}};

    RHIGraphicsPipelineDesc pipelineDesc{};
    pipelineDesc.debugName = "UI.GraphicsPipeline";
    pipelineDesc.layout = pipelineLayout_;
    pipelineDesc.shaders = {vertexShader, fragmentShader};
    pipelineDesc.vertexBuffers.push_back(vertexLayout);
    pipelineDesc.inputAssembly.topology = RHIPrimitiveTopology::TriangleList;
    pipelineDesc.raster.cullMode = RHICullMode::None;
    // UI 已经通过 painter order 决定遮挡关系；关闭深度测试/写入，避免场景深度
    // 让面板在球体后面消失，同时允许 UI pass 在 PBR pass 之后直接叠加。
    pipelineDesc.depthStencil.depthTestEnable = false;
    pipelineDesc.depthStencil.depthWriteEnable = false;
    pipelineDesc.blend.attachments.push_back({
        true,
        RHIBlendFactor::SourceAlpha,
        RHIBlendFactor::OneMinusSourceAlpha,
        RHIBlendOp::Add,
        RHIBlendFactor::One,
        RHIBlendFactor::OneMinusSourceAlpha,
        RHIBlendOp::Add});
    pipelineDesc.colorFormats.push_back(colorFormat_);
    pipelineDesc.depthStencilFormat = depthFormat_;
    try {
        pipeline_ = device_->CreateGraphicsPipeline(pipelineDesc);
    } catch (const std::exception& error) {
        throw std::runtime_error(
            std::string("RHI UI graphics pipeline creation failed: ") + error.what());
    }
}

Context::~Context() {
    Shutdown();
}

void Context::BeginFrame(RHIExtent2D extent, InputState input) {
    // 控件函数是即时提交 API：它们只在当前调用期间生成 CPU 几何，下一帧重新提交。
    // activeSlider_ 例外地跨帧保留，用于在鼠标按住时把拖动状态传递给同一个控件。
    extent_ = extent;
    input_ = input;
    if (!input_.leftButtonDown) {
        activeSlider_ = RHI_INVALID_INDEX;
    }
    nextWidgetId_ = 0;
    nextOrder_ = 0;
    vertices_.clear();
    draws_.clear();
    externalTextureNames_.clear();
}

void Context::AddDrawItem(RHIBindSet bindSet, i32 layer, u32 firstVertex) {
    const u32 vertexCount = static_cast<u32>(vertices_.size()) - firstVertex;
    if (vertexCount != 0) {
        draws_.push_back({bindSet, layer, nextOrder_++, firstVertex, vertexCount});
    }
}

void Context::AddQuad(Rect rect, Color color, RHIBindSet bindSet, i32 layer) {
    if (extent_.width == 0 || extent_.height == 0 || rect.width <= 0.0F || rect.height <= 0.0F) {
        return;
    }
    if (vertices_.size() + 6 > MAX_UI_VERTICES) {
        throw std::runtime_error("UI vertex capacity exceeded");
    }

    // 将像素矩形延迟到 shader 前转换为归一化坐标，避免 UI 控件和 RHI 绑定窗口尺寸。
    const float left = rect.x / static_cast<float>(extent_.width);
    const float top = rect.y / static_cast<float>(extent_.height);
    const float right = (rect.x + rect.width) / static_cast<float>(extent_.width);
    const float bottom = (rect.y + rect.height) / static_cast<float>(extent_.height);
    const float4 vertexColor{color.r, color.g, color.b, color.a};
    const u32 firstVertex = static_cast<u32>(vertices_.size());
    vertices_.insert(vertices_.end(), {
        {{left, top}, {0.0F, 0.0F}, vertexColor},
        {{right, top}, {1.0F, 0.0F}, vertexColor},
        {{right, bottom}, {1.0F, 1.0F}, vertexColor},
        {{left, top}, {0.0F, 0.0F}, vertexColor},
        {{right, bottom}, {1.0F, 1.0F}, vertexColor},
        {{left, bottom}, {0.0F, 1.0F}, vertexColor}});
    AddDrawItem(bindSet, layer, firstVertex);
}

void Context::Panel(Rect rect, Color color, i32 layer) {
    AddQuad(rect, color, whiteBindSet_, layer);
}

void Context::AddTextVertices(
    std::string_view value,
    float x,
    float y,
    float pixelHeight,
    Color color,
    i32 layer) {
    if (pixelHeight <= 0.0F || extent_.width == 0 || extent_.height == 0) {
        return;
    }
    // 每个点阵像素都是一个独立 quad。文字量较大时会较快消耗 MAX_UI_VERTICES，
    // 这是有意的简单实现：它不引入字体 atlas、descriptor array 或额外后端代码。
    const float pixelScale = pixelHeight / 7.0F;
    const float originalX = x;
    const u32 firstVertex = static_cast<u32>(vertices_.size());
    for (char character : value) {
        if (character == '\n') {
            x = originalX;
            y += pixelHeight + pixelScale;
            continue;
        }
        const Glyph& glyph = FindGlyph(character);
        for (u32 row = 0; row < glyph.rows.size(); ++row) {
            for (u32 column = 0; column < 5; ++column) {
                if ((glyph.rows[row] & (1U << (4U - column))) != 0) {
                    if (vertices_.size() + 6 > MAX_UI_VERTICES) {
                        throw std::runtime_error("UI vertex capacity exceeded");
                    }
                    const Rect pixelRect{
                        x + static_cast<float>(column) * pixelScale,
                        y + static_cast<float>(row) * pixelScale,
                        pixelScale,
                        pixelScale};
                    const float left = pixelRect.x / static_cast<float>(extent_.width);
                    const float top = pixelRect.y / static_cast<float>(extent_.height);
                    const float right = (pixelRect.x + pixelRect.width) / static_cast<float>(extent_.width);
                    const float bottom = (pixelRect.y + pixelRect.height) / static_cast<float>(extent_.height);
                    const float4 vertexColor{color.r, color.g, color.b, color.a};
                    vertices_.insert(vertices_.end(), {
                        {{left, top}, {0.0F, 0.0F}, vertexColor},
                        {{right, top}, {1.0F, 0.0F}, vertexColor},
                        {{right, bottom}, {1.0F, 1.0F}, vertexColor},
                        {{left, top}, {0.0F, 0.0F}, vertexColor},
                        {{right, bottom}, {1.0F, 1.0F}, vertexColor},
                        {{left, bottom}, {0.0F, 1.0F}, vertexColor}});
                }
            }
        }
        x += 6.0F * pixelScale;
    }
    AddDrawItem(whiteBindSet_, layer, firstVertex);
}

void Context::Text(std::string_view value, float x, float y, float pixelHeight, Color color, i32 layer) {
    AddTextVertices(value, x, y, pixelHeight, color, layer);
}

void Context::TextBox(Rect rect, std::string_view value, Color background, Color text, i32 layer) {
    Panel(rect, background, layer);
    const float textHeight = std::min(14.0F, std::max(8.0F, rect.height - 10.0F));
    Text(value, rect.x + 7.0F, rect.y + (rect.height - textHeight) * 0.5F, textHeight, text, layer + 1);
}

RHIBindSet Context::BindSetForImage(RHITexture texture, RHITextureView view) {
    if (texture == whiteTexture_ && view == whiteView_) {
        return whiteBindSet_;
    }
    // BindSet 按 (texture, view) 缓存。UI 一帧可能重复显示同一张材质图，缓存可以
    // 避免每次 Image() 都创建原生 descriptor set/descriptor table。
    for (const ImageBinding& binding : imageBindings_) {
        if (binding.texture == texture && binding.view == view) {
            return binding.bindSet;
        }
    }

    RHIBindSetDesc desc{};
    desc.debugName = "UI.ImageBindSet";
    desc.layout = bindSetLayout_;
    RHIResourceBinding resource{};
    resource.binding = 0;
    resource.type = RHIBindingType::CombinedTextureSampler;
    resource.texture = {view, texture};
    resource.sampler = sampler_;
    desc.bindings.push_back(resource);
    const RHIBindSet bindSet = device_->CreateBindSet(desc);
    imageBindings_.push_back({texture, view, bindSet});
    return bindSet;
}

void Context::Image(
    Rect rect,
    RHITexture texture,
    RHITextureView view,
    std::string_view graphResourceName,
    i32 layer) {
    if (!texture || !view) {
        return;
    }
    if (!graphResourceName.empty() &&
        std::find(externalTextureNames_.begin(), externalTextureNames_.end(), graphResourceName) ==
            externalTextureNames_.end()) {
        externalTextureNames_.emplace_back(graphResourceName);
    }
    // 先画一圈蓝色边框，再以 layer+1 画图片，明确图片覆盖边框内部而不覆盖外框。
    Panel({rect.x - 2.0F, rect.y - 2.0F, rect.width + 4.0F, rect.height + 4.0F},
          {0.23F, 0.55F, 0.72F, 0.88F}, layer);
    AddQuad(rect, {1.0F, 1.0F, 1.0F, 1.0F}, BindSetForImage(texture, view), layer + 1);
}

bool Context::Button(std::string_view label, Rect rect, i32 layer) {
    const bool hovered = rect.Contains(input_.mouseX, input_.mouseY);
    Panel(rect, hovered ? Color{0.16F, 0.39F, 0.52F, 0.94F} : Color{0.08F, 0.14F, 0.19F, 0.94F}, layer);
    const float textHeight = 12.0F;
    Text(label,
         rect.x + std::max(7.0F, (rect.width - TextWidth(label, textHeight)) * 0.5F),
         rect.y + (rect.height - textHeight) * 0.5F,
         textHeight,
         {0.94F, 0.98F, 1.0F, 1.0F},
         layer + 1);
    return hovered && input_.leftButtonPressed;
}

bool Context::SliderFloat(
    std::string_view label,
    Rect rect,
    float& value,
    float minimum,
    float maximum,
    float step,
    i32 layer) {
    const u32 widgetId = nextWidgetId_++;
    const Rect track{rect.x, rect.y + 20.0F, rect.width, 7.0F};
    const bool hovered = rect.Contains(input_.mouseX, input_.mouseY);
    if (input_.leftButtonPressed && hovered) {
        activeSlider_ = widgetId;
    }

    bool changed = false;
    if (activeSlider_ == widgetId && input_.leftButtonDown) {
        const float t = std::clamp((input_.mouseX - track.x) / track.width, 0.0F, 1.0F);
        float nextValue = minimum + (maximum - minimum) * t;
        if (step > 0.0F) {
            nextValue = std::round(nextValue / step) * step;
        }
        nextValue = std::clamp(nextValue, minimum, maximum);
        changed = std::abs(nextValue - value) > 0.0001F;
        value = nextValue;
    }

    Panel({track.x, track.y, track.width, track.height}, {0.05F, 0.08F, 0.11F, 0.96F}, layer);
    const float fraction = std::clamp((value - minimum) / (maximum - minimum), 0.0F, 1.0F);
    Panel({track.x, track.y, track.width * fraction, track.height}, {0.18F, 0.67F, 0.81F, 0.98F}, layer + 1);
    Panel({track.x + track.width * fraction - 4.0F, track.y - 4.0F, 8.0F, 15.0F},
          {0.89F, 0.97F, 1.0F, 1.0F}, layer + 2);

    char valueText[32]{};
    std::snprintf(valueText, sizeof(valueText), "%.0f DEG/S", static_cast<double>(value));
    Text(label, rect.x, rect.y, 12.0F, {0.94F, 0.98F, 1.0F, 1.0F}, layer + 3);
    Text(valueText,
         rect.x + std::max(0.0F, rect.width - TextWidth(valueText, 11.0F)),
         rect.y,
         11.0F,
         {0.55F, 0.82F, 0.92F, 1.0F},
         layer + 3);
    return changed;
}

void Context::AppendUploads(RHIFramePacket& frame) {
    // 白纹理只在第一次出现时上传；顶点则每帧覆盖同一个 buffer 的开头区域。
    // RHIFramePacket 持有 std::vector<std::byte>，因此直到提交完成前 CPU 数据仍有效。
    if (staticUploadsPending_) {
        frame.uploads.textures.push_back({
            whiteTexture_,
            0,
            0,
            {},
            {1, 1, 1},
            0,
            0,
            {std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF}}});
        staticUploadsPending_ = false;
    }
    if (!vertices_.empty()) {
        std::vector<std::byte> data(vertices_.size() * sizeof(Vertex));
        std::memcpy(data.data(), vertices_.data(), data.size());
        frame.uploads.buffers.push_back({vertexBuffer_, 0, std::move(data)});
    }
}

void Context::ImportResources(RHIRenderGraphDesc& graph) const {
    // UI buffer/纹理是在 RenderGraph 外创建的，所以必须标记 Imported；图仍能据此
    // 生成状态转换，但不会尝试替 UI 销毁或重新分配这些对象。
    RHIRenderGraphBufferDesc vertices{};
    vertices.name = "UI.Vertices";
    vertices.imported = true;
    vertices.flags = RHIRenderGraphResourceFlags::Imported;
    vertices.externalHandle = vertexBuffer_;
    vertices.desc.size = sizeof(Vertex) * MAX_UI_VERTICES;
    vertices.desc.usage = RHIBufferUsage::Vertex | RHIBufferUsage::TransferDestination;
    graph.buffers.push_back(vertices);

    RHIRenderGraphTextureDesc white{};
    white.name = "UI.White";
    white.imported = true;
    white.flags = RHIRenderGraphResourceFlags::Imported;
    white.externalHandle = whiteTexture_;
    white.desc.extent = {1, 1, 1};
    white.desc.format = RHIFormat::RGBA8_UNorm;
    white.desc.usage = RHITextureUsage::Sampled | RHITextureUsage::TransferDestination;
    graph.textures.push_back(white);
}

void Context::AppendPassReads(RHIRenderGraphPassDesc& pass) const {
    // UI pass 只读顶点和纹理，颜色 attachment 的写入由 PBRDemo 创建 pass 时声明。
    // FragmentShader/VertexInput 阶段信息让 Vulkan barrier 与 D3D12 transition 有依据。
    pass.reads.push_back({
        "UI.Vertices",
        RHIRenderGraphResourceType::Buffer,
        RHIResourceState::VertexBuffer,
        RHIPipelineStage::VertexInput});
    pass.reads.push_back({
        "UI.White",
        RHIRenderGraphResourceType::Texture,
        RHIResourceState::ShaderRead,
        RHIPipelineStage::FragmentShader});
    for (const std::string& name : externalTextureNames_) {
        pass.reads.push_back({
            name,
            RHIRenderGraphResourceType::Texture,
            RHIResourceState::ShaderRead,
            RHIPipelineStage::FragmentShader});
    }
}

void Context::AppendDraws(RHIRenderPassWorkload& workload) {
    // 稳定排序保证“layer 小的先画；同层后提交的后画”。这是 UI 的遮挡规则，
    // 与深度缓冲无关，因此也适用于透明图片和半透明面板。
    std::stable_sort(draws_.begin(), draws_.end(), [](const DrawItem& lhs, const DrawItem& rhs) {
        if (lhs.layer != rhs.layer) {
            return lhs.layer < rhs.layer;
        }
        return lhs.order < rhs.order;
    });
    for (const DrawItem& item : draws_) {
        RHIDrawCommand draw{};
        draw.pipeline = pipeline_;
        draw.bindSets = {item.bindSet};
        draw.vertexStreams = {{vertexBuffer_, 0, 0, sizeof(Vertex)}};
        draw.vertexCount = item.vertexCount;
        draw.firstVertex = item.firstVertex;
        workload.draws.push_back(std::move(draw));
    }
}

void Context::Shutdown() noexcept {
    // Context 的资源必须在 RHIDevice 仍然存活时释放。调用方通常先 WaitIdle，
    // 这里再按 bind set -> pipeline/layout -> view/texture -> buffer 的依赖逆序销毁。
    if (device_ == nullptr) {
        return;
    }
    for (const ImageBinding& binding : imageBindings_) {
        device_->Destroy(binding.bindSet);
    }
    imageBindings_.clear();
    device_->Destroy(pipeline_);
    device_->Destroy(pipelineLayout_);
    device_->Destroy(whiteBindSet_);
    device_->Destroy(bindSetLayout_);
    device_->Destroy(sampler_);
    device_->Destroy(whiteView_);
    device_->Destroy(whiteTexture_);
    device_->Destroy(vertexBuffer_);
    pipeline_ = {};
    pipelineLayout_ = {};
    whiteBindSet_ = {};
    bindSetLayout_ = {};
    sampler_ = {};
    whiteView_ = {};
    whiteTexture_ = {};
    vertexBuffer_ = {};
    device_ = nullptr;
}

} // namespace RHI::UI
