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
    [[nodiscard]] constexpr b8 IsValid() const noexcept { return value != RHI_INVALID_HANDLE_VALUE; }
    [[nodiscard]] explicit constexpr operator b8() const noexcept { return IsValid(); }
    friend constexpr b8 operator==(RHIHandle lhs, RHIHandle rhs) { return lhs.value == rhs.value; }
    friend constexpr b8 operator!=(RHIHandle lhs, RHIHandle rhs) { return lhs.value != rhs.value; }
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
[[nodiscard]] constexpr b8 RHIHasAny(Enum value, Enum flags) noexcept {
    return (RHIEnumToUnderlying(value) & RHIEnumToUnderlying(flags)) != 0;
}

template <typename Enum>
[[nodiscard]] constexpr b8 RHIHasAll(Enum value, Enum flags) noexcept {
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
    b8 enableGPUCrashDumps = false;                                                                       ///< 是否启用 GPU 崩溃转储，具体支持由后端决定。
    b8 enablePipelineCache = true;                                                                        ///< 是否启用管线缓存。
};

struct RHIQueueDesc {
    RHIQueueType queueType = RHIQueueType::Graphics;
    u32 count = 1;
    f32 priority = 1.0F;
};

struct RHIAdapterDesc {
    std::string name;
    RHIGraphicsAPI api = RHIGraphicsAPI::Unknown;
    u64 dedicatedVideoMemory = 0;
    u64 sharedSystemMemory = 0;
    b8 isIntegrated = false;
    b8 isSoftware = false;
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

    R32_UInt,
    R32_SInt,
    R32_Float,
    RG32_UInt,
    RG32_SInt,
    RG32_Float,
    RGB32_UInt,
    RGB32_SInt,
    RGB32_Float,
    RGBA32_UInt,
    RGBA32_SInt,
    RGBA32_Float,

    RGB10A2_UNorm,
    R11G11B10_Float,

    D16_UNorm,
    D24_UNorm,
    S8_UInt,
    D24_UNorm_S8_UInt,
    D32_Float,
    D32_Float_S8_UInt,

    BC1RGBA_UNorm,
    BC1RGBA_SRGB,
    BC3RGBA_UNorm,
    BC3RGBA_SRGB,
    BC5RG_UNorm,
    BC5RG_SRGB,
    BC7RGBA_UNorm,
    BC7RGBA_SRGB,

    ETC2RGB8_UNorm,
    ETC2RGB8_SRGB,
    ETC2RGBA8_UNorm,
    ETC2RGBA8_SRGB,

    ASTC4x4_UNorm,
    ASTC4x4_SRGB,
    ASTC8x8_UNorm,
    ASTC8x8_SRGB,
};

[[nodiscard]] constexpr b8 isDepthFormat(RHIFormat format) noexcept {
    return format == RHIFormat::D16_UNorm ||
           format == RHIFormat::D24_UNorm ||
           format == RHIFormat::D24_UNorm_S8_UInt ||
           format == RHIFormat::D32_Float ||
           format == RHIFormat::D32_Float_S8_UInt; 
}

[[nodiscard]] constexpr b8 hasStencilFormat(RHIFormat format) noexcept {
    return format == RHIFormat::S8_UInt ||
           format == RHIFormat::D24_UNorm_S8_UInt ||
           format == RHIFormat::D32_Float_S8_UInt;
}

enum class RHISampleCount : u8 {
    Count1 = 1,
    Count2 = 2,
    Count4 = 4,
    Count8 = 8,
    Count16 = 16,
    Count32 = 32,
    Count64 = 64
};

enum class RHIPresentMode : u8 {
    Immediate,
    Mailbox,
    FIFO,
    FIFORelaxed
};

enum class RHIResourceState : u16 {
    Undefined,
    Common,
    CopySource,
    CopyDestination,
    VertexBuffer,
    IndexBuffer,
    ConstantBuffer,
    ShaderRead,
    ShaderWrite,
    RenderTarget,
    DepthRead,
    DepthWrite,
    ResolveSource,
    ResolveDestination,
    Present,
    IndirectArgument,
    AccelerationStructureRead,
    AccelerationStructureWrite,
    ShadingRateTexture
};

enum class RHIPipelineStage : u64 {
    None = 0,
    TopOfPipe = 1ull << 0,
    DrawIndirect = 1ull << 1,
    VertexInput = 1ull << 2,
    VertexShader = 1ull << 3,
    TessControlShader = 1ull << 4,
    TessEvaluationShader = 1ull << 5,
    GeometryShader = 1ull << 6,
    FragmentShader = 1ull << 7,
    EarlyFragmentTests = 1ull << 8,
    LateFragmentTests = 1ull << 9,
    ColorAttachmentOutput = 1ull << 10,
    ComputeShader = 1ull << 11,
    Transfer = 1ull << 12,
    BottomOfPipe = 1ull << 13,
    Host = 1ull << 14,
    RayTracingShader = 1ull << 15,
    AccelerationStrctureBuild = 1ull << 16,
    TaskShader = 1ull << 17,
    MeshShader = 1ull << 18,
    AllGraphics = 1ull << 19,
    AllCommands = 1ull << 20
};

template <>
struct RHIEnableEnumFlags<RHIPipelineStage> : std::true_type {};

[[nodiscard]] constexpr RHIPipelineStage operator|(RHIPipelineStage lhs, RHIPipelineStage rhs) noexcept {
    return RHIEnumBitOr(lhs, rhs);
}

[[nodiscard]] constexpr RHIPipelineStage operator&(RHIPipelineStage lhs, RHIPipelineStage rhs) noexcept {
    return RHIEnumBitAnd(lhs, rhs);
}

constexpr RHIPipelineStage& operator|=(RHIPipelineStage& lhs, RHIPipelineStage rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

enum class RHIAccessFlags : u64 {
    None = 0,
    IndirectCommandRead = 1ull << 0,
    IndexRead = 1ull << 1,
    VertexAttributeRead = 1ull << 2,
    UniformRead = 1ull << 3,
    InputAttachmentRead = 1ull << 4,
    ShaderRead = 1ull << 5,
    ShaderWrite = 1ull << 6,
    ColorAttachmentRead = 1ull << 7,
    ColorAttachmentWrite = 1ull << 8,
    DepthStencilRead = 1ull << 9,
    DepthStencilWrite = 1ull << 10,
    TransferRead = 1ull << 11,
    TransferWrite = 1ull << 12,
    HostRead = 1ull << 13,
    HostWrite = 1ull << 14,
    MemoryRead = 1ull << 15,
    MemoryWrite = 1ull << 16,
    AccelerationStructureRead = 1ull << 17,
    AccelerationStructureWrite = 1ull << 18
};

template <>
struct RHIEnableEnumFlags<RHIAccessFlags> : std::true_type {};

[[nodiscard]] constexpr RHIAccessFlags operator|(RHIAccessFlags lhs, RHIAccessFlags rhs) noexcept {
    return RHIEnumBitOr(lhs, rhs);
}

[[nodiscard]] constexpr RHIAccessFlags operator&(RHIAccessFlags lhs, RHIAccessFlags rhs) noexcept {
    return RHIEnumBitAnd(lhs, rhs);
}

constexpr RHIAccessFlags& operator|=(RHIAccessFlags& lhs, RHIAccessFlags rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

struct RHIExtent2D {
    u32 width = 1;
    u32 height = 1;
};

struct RHIExtent3D {
    u32 width = 1;
    u32 height = 1;
    u32 depth = 1;
};

struct RHIOffset2D {
    i32 x = 0;
    i32 y = 0;
};

struct RHIOffset3D {
    i32 x = 0;
    i32 y = 0;
    i32 z = 0;
};

struct RHIRect2D {
    RHIOffset2D offset{};
    RHIExtent2D extent{};
};

struct RHIViewport {
    f32 x = 0.0F;
    f32 y = 0.0F;
    f32 width = 0.0F;
    f32 height = 0.0F;
    f32 minDepth = 0.0F;
    f32 maxDepth = 0.0F;
};

struct RHIClearColor {
    f32 r = 0.0F;
    f32 g = 0.0F;
    f32 b = 0.0F;
    f32 a = 1.0F;
};

struct RHIClearDepthStencil {
    f32 depth = 0.0F;
    u32 stencil = 0;
};

struct RHIClearValue {
    RHIClearColor color{};
    RHIClearDepthStencil depthStencil{};
};

enum class RHITextureAspect : u32 {
    None = 0,
    Color = 1u << 0,
    Depth = 1u << 1,
    Stencil = 1u << 2,
    Plane0 = 1u << 3,
    Plane1 = 1u << 4,
    Plane2 = 1u << 5,
    All = 0xFFFFFFFFu
};

template <>
struct RHIEnableEnumFlags<RHITextureAspect> : std::true_type {};

[[nodiscard]] constexpr RHITextureAspect operator|(RHITextureAspect lhs, RHITextureAspect rhs) noexcept {
    return RHIEnumBitOr(lhs, rhs);
}

[[nodiscard]] constexpr RHITextureAspect operator&(RHITextureAspect lhs, RHITextureAspect rhs) noexcept {
    return RHIEnumBitAnd(lhs, rhs);
}

constexpr RHITextureAspect& operator|=(RHITextureAspect& lhs, RHITextureAspect rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

enum class RHIBufferUsage : u32 {
    None = 0,
    TransferSource = 1u << 0,
    TransferDestination = 1u << 1,
    Vertex = 1u << 2,
    Index = 1u << 3,
    Uniform = 1u << 4,
    Storage = 1u << 5,
    Indirect = 1u << 6,
    ShaderDeviceAddress = 1u << 7
};

template <>
struct RHIEnableEnumFlags<RHIBufferUsage> : std::true_type {};

[[nodiscard]] constexpr RHIBufferUsage operator|(RHIBufferUsage lhs, RHIBufferUsage rhs) noexcept {
    return RHIEnumBitOr(lhs, rhs);
}

[[nodiscard]] constexpr RHIBufferUsage operator&(RHIBufferUsage lhs, RHIBufferUsage rhs) noexcept {
    return RHIEnumBitAnd(lhs, rhs);
}

constexpr RHIBufferUsage& operator|=(RHIBufferUsage& lhs, RHIBufferUsage rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

enum class RHIBufferCreateFlags : u32 {
    None = 0,
    DedicatedMemory = 1u << 0,
    SparseBinding = 1u << 1,
    RingBuffer = 1u << 2,
    Transient = 1u << 3
};

template <>
struct RHIEnableEnumFlags<RHIBufferCreateFlags> : std::true_type {};

[[nodiscard]] constexpr RHIBufferCreateFlags operator|(RHIBufferCreateFlags lhs, RHIBufferCreateFlags rhs) noexcept {
    return RHIEnumBitOr(lhs, rhs);
}

[[nodiscard]] constexpr RHIBufferCreateFlags operator&(RHIBufferCreateFlags lhs, RHIBufferCreateFlags rhs) noexcept {
    return RHIEnumBitAnd(lhs, rhs);
}

constexpr RHIBufferCreateFlags& operator|=(RHIBufferCreateFlags& lhs, RHIBufferCreateFlags rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

enum class RHITextureUsage : u32 {
    None = 0,
    TransferSource = 1u << 0,
    TransferDestination = 1u << 1,
    Sampled = 1u << 2,
    Storage = 1u << 3,
    ColorAttachment = 1u << 4,
    DepthStencilAttachment = 1u << 5,
    Present = 1u << 6,
    Transient = 1u << 7,
};

template <>
struct RHIEnableEnumFlags<RHITextureUsage> : std::true_type {};

[[nodiscard]] constexpr RHITextureUsage operator|(RHITextureUsage lhs, RHITextureUsage rhs) noexcept {
    return RHIEnumBitOr(lhs, rhs);
}

[[nodiscard]] constexpr RHITextureUsage operator&(RHITextureUsage lhs, RHITextureUsage rhs) noexcept {
    return RHIEnumBitAnd(lhs, rhs);
}

constexpr RHITextureUsage& operator|=(RHITextureUsage& lhs, RHITextureUsage rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

enum class RHITextureCreateFlags : u32 {
    None = 0,
    CubeCompatible = 1u << 0,
    MutableFormat = 1u << 1,
    DedicatedMemory = 1u << 2,
    SparseBinding = 1u << 3,
    GenerateMips = 1u << 4,
    RenderGraphTransient = 1u << 5
};

template <>
struct RHIEnableEnumFlags<RHITextureCreateFlags> : std::true_type {};

[[nodiscard]] constexpr RHITextureCreateFlags operator|(RHITextureCreateFlags lhs, RHITextureCreateFlags rhs) noexcept {
    return RHIEnumBitOr(lhs, rhs);
}

[[nodiscard]] constexpr RHITextureCreateFlags operator&(RHITextureCreateFlags lhs, RHITextureCreateFlags rhs) noexcept {
    return RHIEnumBitAnd(lhs, rhs);
}

constexpr RHITextureCreateFlags& operator|=(RHITextureCreateFlags& lhs, RHITextureCreateFlags rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

enum class RHIMemoryUsage : u8 {
    GpuOnly,
    CpuToGpu,
    GpuToCpu,
    CpuOnly,
};

using RHIMemory = RHIMemoryUsage;

enum class RHIResourceLifetime : u8 {
    Persistent,
    PerFrame,
    Transient,
};

enum class RHITextureDimension : u8 {
    Texture1D,
    Texture2D,
    Texture3D
};

enum class RHITextureViewDimension : u8 {
    View1D,
    View1DArray,
    View2D,
    View2DArray,
    View3D,
    View3DArray,
    Cube,
    CubeArray
};

enum class RHIFilterMode : u8 {
    Nearest,
    Linear
};

enum class RHIMipmapMode : u8 {
    Nearest,
    Linear,
};

enum class RHIAddressMode : u8 {
    Repeat,
    MirroredRepeat,
    ClampToEdge,
    ClampToBorder,
};

enum class RHIBorderColor : u8 {
    TransparentBlack,
    OpaqueBlack,
    OpaqueWhite,
};

enum class RHICompareOp : u8 {
    Never,
    Less,
    LessOrEqual,
    Greater,
    GreaterOrEqual,
    Equal,
    NotEqual,
    Always
};

struct RHIBufferDesc {
    std::string debugName;
    u64 size = 0;
    RHIBufferUsage usage = RHIBufferUsage::None;
    RHIBufferCreateFlags flags = RHIBufferCreateFlags::None;
    RHIMemoryUsage memoryUsage = RHIMemoryUsage::GpuOnly;
    RHIResourceLifetime lifetime = RHIResourceLifetime::Persistent;
    b8 persistentlyMapped = false;
};

struct RHITextureDesc {
    std::string debugName;
    RHITextureDimension dimension = RHITextureDimension::Texture2D;
    RHIExtent3D extent{};
    u32 arrayLayers = 1;
    u32 mipLevel = 1;
    RHIFormat format = RHIFormat::RGBA8_UNorm;
    RHISampleCount samples = RHISampleCount::Count1;
    RHITextureUsage usage = RHITextureUsage::Sampled;
    RHITextureCreateFlags flags = RHITextureCreateFlags::None;
    RHIResourceLifetime linetime = RHIResourceLifetime::Persistent;
    RHIResourceState initialState = RHIResourceState::Undefined;
};

struct RHITextureViewDesc {
    std::string debugName;
    RHITexture texture{};
    RHITextureViewDimension dimension = RHITextureViewDimension::View2D;
    RHIFormat format = RHIFormat::Undefined;
    RHITextureAspect aspect = RHITextureAspect::Color;
    u32 baseMipLevel = 0;
    u32 mipLevelCount = 1;
    u32 baseArrayLayer = 0;
    u32 arrayLayerCount = 1;
};

struct RHISamplerDesc {
    std::string debugName;
    RHIFilterMode minFilter = RHIFilterMode::Linear;
    RHIFilterMode magFilter = RHIFilterMode::Linear;
    RHIMipmapMode mipmapMode = RHIMipmapMode::Linear;
    RHIAddressMode addressU = RHIAddressMode::Repeat;
    RHIAddressMode addressV = RHIAddressMode::Repeat;
    RHIAddressMode addressW = RHIAddressMode::Repeat;
    f32 mipLodBias = 0.0F;
    f32 minLod = 0.0F;
    f32 maxLod = std::numeric_limits<f32>::max();
    b8 samplerAnisotropy = false;
    f32 maxAnisotropy = 1.0F;
    b8 enableCompare = false;
    RHICompareOp compareOp = RHICompareOp::LessOrEqual;
    RHIBorderColor borderColor = RHIBorderColor::OpaqueBlack;
};

enum class RHIShaderStage : u32 {
    None = 0,
    Vertex = 1u << 0,
    TessControl = 1u << 1,
    TessEvaluation = 1u << 2,
    Geometry = 1u << 3,
    Fragment = 1u << 4,
    Compute = 1u << 5,
    Task = 1u << 6,
    Mesh = 1u << 7,
    AllGraphics = (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) | (1u << 4) | (1u << 6) | (1u << 7),
    All = 0xFFFFFFFFu
};

template <>
struct RHIEnableEnumFlags<RHIShaderStage> : std::true_type {};

[[nodiscard]] constexpr RHIShaderStage operator|(RHIShaderStage lhs, RHIShaderStage rhs) noexcept {
    return RHIEnumBitOr(lhs, rhs);
}

[[nodiscard]] constexpr RHIShaderStage operator&(RHIShaderStage lhs, RHIShaderStage rhs) noexcept {
    return RHIEnumBitAnd(lhs, rhs);
}

constexpr RHIShaderStage& operator|=(RHIShaderStage& lhs, RHIShaderStage rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

enum class RHIShaderLanguage : u8 {
    Unknown,
    GLSL,
    HLSL,
    SLang,
    MSL,
    SPIRV,
    DXIL
};

struct RHIShaderDefine {
    std::string name;
    std::string value = "1";
};

struct RHIShaderCompileOptions {
    std::string targetProfile;
    std::vector<std::string> includeDirectories;
    std::vector<RHIShaderDefine> defines;
    b8 enableDebugInfo = true;
    b8 optimize = true;
    b8 treatWarningAsErrors = false;
};

struct RHIShaderDesc {
    std::string debugName;
    RHIShaderStage stage = RHIShaderStage::Vertex;
    RHIShaderLanguage language = RHIShaderLanguage::Unknown;
    std::string entryPoint = "main";
    std::string filePath;
    std::string source;
    std::vector<std::byte> byteCode;
    RHIShaderCompileOptions compileOptions{};
};

struct RHIShaderSpecializationConstant {
    u32 contantId = 0;
    std::vector<std::byte> data;
};

enum class RHIBindingType : u8 {
    UniformBuffer,
    StorageBuffer,
    SampledTexture,
    StorageTexture,
    Sampler,
    CombinedTextureSampler,
    PushConstant,
    AccelerationStructure,
};

enum class RHITextureSampleType : u8 {
    Float,
    UnfilterableFloat,
    SignedInteger,
    UnsignedInteger,
    Depth,
};

struct RHIBindSetLayoutEntry {
    u32 binding = 0;
    RHIBindingType type = RHIBindingType::UniformBuffer;
    RHIShaderStage visibility = RHIShaderStage::AllGraphics;
    u32 arrayCount = 1;
    b8 writable = true;
    RHITextureViewDimension textureViewDimension = RHITextureViewDimension::View2D;
    RHITextureSampleType textureSampleType = RHITextureSampleType::Float;
    RHIFormat storageTextureFormat = RHIFormat::Undefined;
};

struct RHIBindLayoutDesc {
    std::string debugName;
    u32 set = 0;
    std::vector<RHIBindSetLayoutEntry> entries;
};

struct RHIBufferBinding {
    RHIBuffer buffer{};
    u64 offset = 0;
    u64 size = RHI_WHOLE_SIZE;
};

struct RHITextureBinding {
    RHITextureView view{};
    RHITexture texture{};
};

struct RHIResourceBinding {
    u32 binding = 0;
    u32 arrayElement = 0;
    RHIBindingType type = RHIBindingType::UniformBuffer;
    RHIBufferBinding buffer{};
    RHITextureBinding texture{};
    RHISampler sampler{};
};

struct RHIBindSetDesc {
    std::string debugName;
    RHIBindSetLayout layout{};
    std::vector<RHIResourceBinding> binding;
};

struct RHIPushConstantRange {
    RHIShaderStage stages = RHIShaderStage::AllGraphics;
    u32 offset = 0;
    u32 size = 0;
};

struct RHIPipelineLayoutDesc {
    std::string debugName;
    std::vector<RHIBindSetLayout> bindSetLayouts;
    std::vector<RHIPushConstantRange> pushConstant;
};

struct RHIShaderResourceReflection {
    std::string name;
    u32 set = 0;
    u32 binding = 0;
    RHIBindingType type = RHIBindingType::UniformBuffer;
    RHIShaderStage stage = RHIShaderStage::None;
    u32 arrayCount = 1;
    u32 size = 0;
};

struct RHIShaderParameterReflection {
    std::string name;
    std::string semanticName;
    u32 semanticIndex = 0;
    u32 location = 0;
    RHIFormat format = RHIFormat::Undefined;
};

struct RHIShaderReflectionDesc {
    std::vector<RHIShaderResourceReflection> resources;
    std::vector<RHIShaderParameterReflection> inputs;
    std::vector<RHIShaderParameterReflection> outputs;
    std::vector<RHIPushConstantRange> pushConstants;
};

enum class RHIVertexFormat : u8 {
    Float32,
    Float32x2,
    Float32x3,
    Float32x4,
    UInt32,
    UInt32x2,
    UInt32x3,
    UInt32x4,
    SInt32,
    SInt32x2,
    SInt32x3,
    SInt32x4,
    UNorm8x4,
    SNorm8x4,
    UInt16x2,
    UInt16x4,
    SInt16x2,
    SInt16x4,
    UNorm16x2,
    UNorm16x4,
    SNorm16x2,
    SNorm16x4
};

enum class RHIVertexInputRate : u8 {
    PerVertex,
    PerInstance
};

} // namespace RHI