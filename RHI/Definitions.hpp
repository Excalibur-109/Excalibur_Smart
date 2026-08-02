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
    static_assert(std::is_enum_v<Enum>, "RHIEnumToUnderlying requires an enum type");
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

struct RHIVertexAttributeDesc {
    std::string semanticName;
    u32 semanticIndex = 0;
    u32 location = 0;
    u32 binding = 0;
    RHIVertexFormat format = RHIVertexFormat::Float32x3;
    u64 offset = 0;
};

struct RHIVertexBufferLayoutDesc {
    u32 binding = 0;
    u32 stride = 0;
    RHIVertexInputRate inputRate = RHIVertexInputRate::PerVertex;
    u32 stepRate = 1;
    std::vector<RHIVertexAttributeDesc> attributes;
};

enum class RHIPrimitiveTopology : u8 {
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip,
    PatchList       ///< patch 图元，用于 tessellation 阶段，控制点数量由管线状态指定。
};

enum class RHIPolygonMode : u8 {
    Fill,
    Line,
    Point
};

enum class RHICullMode : u8 {
    None,
    Front,
    Back,
    FrontAndBack
};

enum class RHIFrontFace : u8 {
    CounterClockWise,
    ClockWise
};

enum class RHIStencilOp : u8 {
    Keep,                       ///< 保留当前 stencil 值不变。
    Zero,                       ///< 将 stencil 值写为 0。
    Replace,                    ///< 将 stencil 值替换为 reference 值。
    IncrementClamp,             ///< stencil 值加 1，并在最大值处饱和。
    DecrementClamp,             ///< stencil 值减 1，并在 0 处饱和。
    Invert,                     ///< 按位反转 stencil 值。
    IncrementWrap,              ///< stencil 值加 1，超过最大值后回绕到 0。
    DecrementWarp               ///< stencil 值减 1，低于 0 后回绕到最大值。
};

enum class RHIBlendFactor : u8 {
    Zero,
    One,
    SourceColor,
    OneMinusSourceColor,
    DestinationColor,
    OneMinusDestinationColor,
    SourceAlpha,
    OneMinusSourceAlpha,
    DestinationAlpha,
    OneMinusDestinationAlpha,
    ConstantColor,
    OneMinusConstantColor,
    ConstantAlpha,
    OneMinusConstantAlpha,
};

enum class RHIBlendOp : u8 {
    Add,
    Subtract,
    ReverseSubtract,
    Min,
    Max
};

enum class RHILogicOp : u8 {
    Clear,              ///< 输出全 0，忽略源和目标颜色。
    And,                ///< 输出 source AND destination。
    AndReverse,         ///< 输出 source AND (NOT destination)。
    Copy,               ///< 输出 source，等价于直接写入源颜色。
    AndInverted,        ///< 输出 (NOT source) AND destination。
    NoOp,               ///< 保留 destination，不写入源颜色。
    Xor,                ///< 输出 source XOR destination。
    Or,                 ///< 输出 source OR destination。
    Nor,                ///< 输出 NOT (source OR destination)。
    Equivalent,         ///< 输出 NOT (source XOR destination)，即逐位等价。
    Invert,             ///< 输出 NOT destination，反转目标颜色位。
    OrReverse,          ///< 输出 source OR (NOT destination)。
    CopyInverted,       ///< 输出 NOT source。
    OrInverted,         ///< 输出 (NOT source) OR destination。
    Nand,               ///< 输出 NOT (source AND destination)。
    Set                 ///< 输出全 1，忽略源和目标颜色。
};

enum class RHIColorWriteMask : u8 {
    None = 0,
    R = 1u << 0,
    G = 1u << 1,
    B = 1u << 2,
    A = 1u << 3,
    All = 0x0F
};

template <>
struct RHIEnableEnumFlags<RHIColorWriteMask> : std::true_type {};

[[nodiscard]] constexpr RHIColorWriteMask operator|(RHIColorWriteMask lhs, RHIColorWriteMask rhs) noexcept {
    return RHIEnumBitOr(lhs, rhs);
}

[[nodiscard]] constexpr RHIColorWriteMask operator&(RHIColorWriteMask lhs, RHIColorWriteMask rhs) noexcept {
    return RHIEnumBitAnd(lhs, rhs);
}

constexpr RHIColorWriteMask& operator|=(RHIColorWriteMask& lhs, RHIColorWriteMask rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

enum class RHIDynamicState : u8 {
    Viewport,
    Scissor,
    LineWidth,
    DepthBias,
    BlendConstants,
    StencilReference
};

struct RHIInputAssemblyState {
    RHIPrimitiveTopology topology = RHIPrimitiveTopology::TriangleList;
    b8 primitiveRestart = false;
    u32 patchControlPoints = 0;
};

struct RHIRasterState {
    RHIPolygonMode polygonMode = RHIPolygonMode::Fill;
    RHICullMode cullMode = RHICullMode::Back;
    RHIFrontFace frontFace = RHIFrontFace::CounterClockWise;
    b8 depthClampEnable = false;
    b8 depthBiasEnable = false;
    f32 depthBiasConstantFactor = 0.0F;
    f32 depthBiasClamp = 0.0F;
    f32 depthBiasSlopeFactor = 0.0F;
    f32 lineWidth = 1.0F;
};

struct RHIStencilFaceState {
    RHIStencilOp failOp = RHIStencilOp::Keep;
    RHIStencilOp passOp = RHIStencilOp::Keep;
    RHIStencilOp depthFailOp = RHIStencilOp::Keep;
    RHICompareOp compareOp = RHICompareOp::Always;
    u32 compareMask = 0xFFFFFFFFu;
    u32 writeMask = 0xFFFFFFFFu;
    u32 reference = 0;
};

struct RHIDepthStencilState {
    b8 depthTestEnable = true;
    b8 depthWriteEnable = true;
    RHICompareOp depthCompareOp = RHICompareOp::Less;
    b8 depthBoundsTestEnable = false;
    f32 minDepthBounds = 0.0F;
    f32 maxDepthBounds = 1.0F;
    b8 stencilTestEnable = false;
    RHIStencilFaceState front{};
    RHIStencilFaceState back{};
};

struct RHIMultisampleState {
    RHISampleCount samples = RHISampleCount::Count1;
    b8 sampleShadingEnable = false;
    f32 minSampleShading = 1.0F;
    u64 sampleMask = std::numeric_limits<u64>::max();
    b8 alphaToCoverageEnable = false;
    b8 alphaToOneEnable = false;
};

struct RHIColorBlendAttachmentState {
    b8 blendEnable = false;
    RHIBlendFactor sourceColor = RHIBlendFactor::One;
    RHIBlendFactor destinationColor = RHIBlendFactor::Zero;
    RHIBlendOp colorOp = RHIBlendOp::Add;
    RHIBlendFactor sourceAlpha = RHIBlendFactor::One;
    RHIBlendFactor destinationAlpha = RHIBlendFactor::Zero;
    RHIBlendOp blendOp = RHIBlendOp::Add;
    RHIColorWriteMask writeMask = RHIColorWriteMask::All;
};

struct RHIBlendState {
    b8 loginOpEnable = false;
    RHILogicOp logicOp = RHILogicOp::Copy;
    std::array<f32, 4> blendConstants{0.0F, 0.0F, 0.0F, 0.0F};
    std::vector<RHIColorBlendAttachmentState> attachments;
};

struct RHIGraphicsPipelineDesc {
    std::string debugName;
    RHIPipelineCache cache{};
    RHIPipelineLayout layout{};
    std::vector<RHIShaderDesc> shaders;
    std::vector<RHIShaderSpecializationConstant> specializationConstants;
    std::vector<RHIVertexBufferLayoutDesc> vertexBuffers;
    RHIInputAssemblyState inputAssembly{};
    RHIRasterState raster{};
    RHIDepthStencilState depthStencil{};
    RHIMultisampleState multisample{};
    RHIBlendState blend{};
    std::vector<RHIDynamicState> dynamicState{RHIDynamicState::Viewport, RHIDynamicState::Scissor};
    std::vector<RHIFormat> colorFormats;
    RHIFormat depthStencilFormat = RHIFormat::Undefined;
    RHIRenderPass compatibleRenderPass{};
    u32 subpass = 0;
};

struct RHIComputePipelineDesc {
    std::string debugName;
    RHIPipelineCache cache{};
    RHIPipelineLayout layout{};
    RHIShaderDesc shader{};
    std::vector<RHIShaderSpecializationConstant> specializationConstants;
};

struct RHIPipelineCacheDesc {
    std::string debugName;
    std::vector<std::byte> initailData;
    b8 allowSerialization = true;
};

enum class RHIQueryType : u8 {
    Timestamp,              ///< GPU 时间戳查询，用于测量 pass 或命令区间的 GPU 执行时间。
    Occlusion,              ///< 遮挡查询，用于统计通过深度/模板测试的样本数或可见性。
    PipelineStatistics,     ///< 管线统计查询，用于统计 shader 调用、图元数量等性能计数。
};

enum class RHIPipelineStatisticFlags : u32 {
    None                            = 0,        ///< 不启用任何管线统计项。
    InputAssemblyVertices           = 1u << 0,  ///< 统计输入装配阶段读取的顶点数量。
    InputAssemblyPrimitives         = 1u << 1,  ///< 统计输入装配阶段生成的图元数量。
    VertexShaderInvocations         = 1u << 2,  ///< 统计 vertex shader 调用次数。
    GeometryShaderInvocations       = 1u << 3,  ///< 统计 geometry shader 调用次数。
    GeometryShaderPrimitives        = 1u << 4,  ///< 统计 geometry shader 输出的图元数量。
    ClippingInvocations             = 1u << 5,  ///< 统计进入裁剪阶段的图元数量。
    ClippingPrimitives              = 1u << 6,  ///< 统计通过裁剪并继续光栅化的图元数量。
    FragmentShaderInvocations       = 1u << 7,  ///< 统计 fragment/pixel shader 调用次数。
    TessControlShaderPatches        = 1u << 8,  ///< 统计 tessellation control shader 处理的 patch 数量。
    TessEvaluationShaderInvocations = 1u << 9,  ///< 统计 tessellation evaluation shader 调用次数。
    ComputeShaderInvocations        = 1u << 10  ///< 统计 compute shader invocation 数量。
};

template <>
struct RHIEnableEnumFlags<RHIPipelineStatisticFlags> : std::true_type {};

[[nodiscard]] constexpr RHIPipelineStatisticFlags operator|(RHIPipelineStatisticFlags lhs, RHIPipelineStatisticFlags rhs) noexcept {
    return RHIEnumBitOr(lhs, rhs);
}

[[nodiscard]] constexpr RHIPipelineStatisticFlags operator&(RHIPipelineStatisticFlags lhs, RHIPipelineStatisticFlags rhs) noexcept {
    return RHIEnumBitAnd(lhs, rhs);
}

constexpr RHIPipelineStatisticFlags& operator|=(RHIPipelineStatisticFlags& lhs, RHIPipelineStatisticFlags rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

struct RHIQueryPoolDesc {
    std::string debugName;
    RHIQueryType type = RHIQueryType::Timestamp;
    u32 queryCount = 1;
    RHIPipelineStatisticFlags statistics = RHIPipelineStatisticFlags::None;
};

enum class RHILoadOp : u8 {
    Load,
    Clear,
    DontCare
};

enum class RHIStoreOp : u8 {
    Store,
    DontCare
};

/// MSAA resolve 行为。Average 是最常见的颜色 resolve；深度 resolve 后端支持差异较大。
enum class RHIResolveMode : u8 {
    None,
    Average,
    Min,
    Max,
    SampleZero
};

struct RHIRenderTargetDesc {
    RHITextureView view{};
    RHITextureView resolveView{};
    RHIResolveMode resolveMode = RHIResolveMode::None;
    RHILoadOp loadOp = RHILoadOp::Load;
    RHIStoreOp storeOp = RHIStoreOp::Store;
    RHIClearColor clearColor{};
    RHIResourceState stateBefore = RHIResourceState::Undefined;
    RHIResourceState stateAfter = RHIResourceState::RenderTarget;
};

struct RHIDepthStencilTargetDesc {
    RHITextureView view{};
    RHITextureView depthResolveView{};
    RHITextureView stencilResolveView{};
    RHIResolveMode depthResolveMode = RHIResolveMode::None;
    RHIResolveMode stencilResolveMode = RHIResolveMode::None;
    RHILoadOp depthLoadOp = RHILoadOp::Load;
    RHIStoreOp depthStoreOp = RHIStoreOp::Store;
    RHILoadOp stencilLoadOp = RHILoadOp::DontCare;
    RHIStoreOp stencilStoreOp = RHIStoreOp::DontCare;
    RHIClearDepthStencil clearValue{};
    RHIResourceState stateBefore = RHIResourceState::Undefined;
    RHIResourceState stateAfter = RHIResourceState::DepthWrite;
};

struct RHIRenderPassDesc {
    std::string debugName;
    RHIRect2D renderArea{};
    std::vector<RHIRenderTargetDesc> colorTargets;
    std::optional<RHIDepthStencilTargetDesc> depthStencilTarget;
};

struct RHIFrameBufferDesc {
    std::string debugName;
    RHIRenderPass renderPass{};
    std::vector<RHITextureView> attachments;
    RHIExtent2D extent{};
    u32 layers = 1;
};

enum class RHIColorSpace : u8 {
    SRGBNonlinear,      ///< 标准 sRGB 非线性色彩空间，普通 SDR swapchain 默认选择。
    DisplayP3Nonlinear, ///< Display P3 非线性色彩空间，适合广色域 SDR 输出。
    ExtendSGBLinear,    ///< 扩展 sRGB 线性色彩空间，适合宽范围线性颜色输出。
    HDR10ST2084,        ///< HDR10 PQ/ST2084 色彩空间，适合 HDR10 显示链路。
    HDR10HLG            ///< HDR HLG 色彩空间，适合广播或 HLG HDR 输出。
};

enum class RHISurfaceTransform : u8 {
    Identity,
    Rotate90,
    Rotate180,
    Rotate270,
    HorizontalMirrored,
    HorizontalMirroredRotate90,
    HorizontalMirroredRotate180,
    HorizontalMirroredRotate270,
    Inherit
};

enum class RHICompositeAlphaMode : u8 {
    Opaque,
    Premultiplied,
    PostMultiplied,
    Inherit
};

struct RHISwapchainDesc {
    std::string debugName;
    RHIExtent2D extent{};
    RHIFormat preferredFormat = RHIFormat::BGRA_SRGB;
    RHIColorSpace colorSpace = RHIColorSpace::SRGBNonlinear;
    RHIPresentMode presentMode = RHIPresentMode::FIFO;
    u32 imageCount = 2;
    RHISurfaceTransform preTransform = RHISurfaceTransform::Identity;
    RHICompositeAlphaMode compositeAlpha = RHICompositeAlphaMode::Opaque;
    bool allowTearing = false;
    bool fullscreen = false;
    bool hdr = false;
};

struct RHITextureSubresourceRange {
    RHITextureAspect asect = RHITextureAspect::All;
    u32 baseMipLevel = 0;
    u32 mipLevelCount = 1;
    u32 baseArrayLayer = 0;
    u32 arrayLayersCount = 1;
};

struct RHIGlobalBarrier {
    RHIPipelineStage sourceStage = RHIPipelineStage::AllCommands;
    RHIPipelineStage destinationStage = RHIPipelineStage::AllCommands;
    RHIAccessFlags sourceFlags = RHIAccessFlags::MemoryWrite;
    RHIAccessFlags destinationFlags = RHIAccessFlags::MemoryRead;
};

struct RHITextureBarrier {
    RHITexture texture{};
    RHITextureSubresourceRange range{};
    RHIResourceState before = RHIResourceState::Undefined;
    RHIResourceState after = RHIResourceState::Common;
    RHIPipelineStage sourceStages = RHIPipelineStage::AllCommands;
    RHIPipelineStage destinationStages = RHIPipelineStage::AllCommands;
    RHIAccessFlags sourceAccess = RHIAccessFlags::None;
    RHIAccessFlags destinationAccess = RHIAccessFlags::None;
    RHIQueueType sourceQueue = RHIQueueType::Graphics;
    RHIQueueType destinationQueue = RHIQueueType::Graphics;
    bool discardContents = false;
};

struct RHIBufferBarrier {
    RHIBuffer buffer{};
    u64 offset = 0;
    u64 size = RHI_WHOLE_SIZE;
    RHIResourceState before = RHIResourceState::Undefined;
    RHIResourceState after = RHIResourceState::Common;
    RHIPipelineStage sourceStages = RHIPipelineStage::AllCommands;
    RHIPipelineStage destinationStages = RHIPipelineStage::AllCommands;
    RHIAccessFlags sourceAccess = RHIAccessFlags::None;
    RHIAccessFlags destinationAccess = RHIAccessFlags::None;
    RHIQueueType sourceQueue = RHIQueueType::Graphics;
    RHIQueueType destinationQueue = RHIQueueType::Graphics;
};

struct RHIResourceBarriers {
    std::vector<RHIGlobalBarrier> globals;
    std::vector<RHITextureBarrier> textures;
    std::vector<RHIBufferBarrier> buffers;
};

struct RHIBufferUploadDesc {
    RHIBuffer destination{};
    u64 destinationOffset = 0;
    std::vector<std::byte> data;
};

struct RHITextureUploadDesc {
    RHITexture destination{};
    u32 mipLevel = 0;
    u32 arrayLayer = 0;
    RHIOffset3D offset{};
    RHIOffset3D extent{};
    u64 bytesPerRow = 0;
    u64 rowsPerImage = 0;
    std::vector<std::byte> data;
};

struct RHIUploadBatchDesc {
    std::vector<RHIBufferUploadDesc> buffers;
    std::vector<RHITextureUploadDesc> textures;
};

struct RHIBufferCopyDesc {
    RHIBuffer source{};
    RHIBuffer destination{};
    u64 sourceOffset = 0;
    u64 destinationOffset = 0;
    u64 size = 0;
};

struct RHITextureCopyLocation {
    RHITexture texture{};
    RHITextureAspect aspect = RHITextureAspect::Color;
    u32 mipLevel = 0;
    u32 arrayLayer = 0;
    RHIOffset3D offset{};
};

struct RHITextureCopyDesc {
    RHITextureCopyLocation source{};
    RHITextureCopyLocation destination{};
    RHIExtent3D extent{};
};

struct RHIBufferTextureCopyDesc {
    RHIBuffer buffer{};
    RHITextureCopyLocation texture{};
    u64 bufferOffset = 0;
    u64 bytesPerRow = 0;
    u64 rowsPerImage = 0;
    RHIExtent3D extent{};
};

struct RHITextureBlitDesc {
    RHITextureCopyLocation source{};
    RHITextureCopyLocation destination{};
    RHIExtent3D sourceExtent{};
    RHIExtent3D destinationExtent{};
    RHIFilterMode filter = RHIFilterMode::Linear;
};

struct RHIMipmapGenerationDesc {
    RHITexture texture{};
    RHITextureAspect aspect = RHITextureAspect::Color;
    u32 baseArrayLayer = 0;
    u32 arrayLayerCount = 0;
    RHIFilterMode filter = RHIFilterMode::Linear;
};

enum class RHIIndexType : u8 {
    UInt16,
    UInt32,
};

struct RHIVertexStream {
    RHIBuffer buffer{};
    u32 binding = 0;
    u64 offset = 0;
    u64 stride = 0;
};

struct RHIIndexStream {
    RHIBuffer buffer{};
    RHIIndexType indexType = RHIIndexType::UInt32;
    u64 offset = 0;
    u32 indexCount = 0;
};

struct RHIBoundingBox {
    float3 min{0.0F};
    float3 max{0.0F};
};

struct RHIBoundingSphere {
    float3 center{0.0F};
    float radius = 0.0F;
};

struct RHISubmeshDesc {
    std::string name;
    u32 firstVertex = 0;
    u32 vertexCount = 0;
    u32 firstIndex = 0;
    u32 indexCount = 0;
    u32 firstInstance = 0;
    u32 instanceCount = 1;
    i32 materialIndex = -1;
    RHIBoundingBox boundsBox{};
    RHIBoundingSphere boundsSphere{};
};

struct RHIMeshDesc {
    std::string debugName;
    std::vector<RHIVertexStream> vertexStream;
    std::vector<RHIIndexStream> indexStream;
    std::vector<RHISubmeshDesc> submeshes;
};

struct RHITextureSlot {
    std::string name;
    RHITextureView texture{};
    RHISampler sampler{};
};

enum class RHIMaterialParameterType : u8 {
    Float,
    Float2,
    Float3,
    Float4,
    Int,
    UInt,
    Bool
};

struct RHIMaterialParameter {
    std::string name;
    RHIMaterialParameterType type = RHIMaterialParameterType::Float4;
    float4 value{0.0F};
};

struct RHIMaterialDesc {
    std::string debugName;
    RHIPipeline pipeline{};
    std::vector<RHIBindSet> bindSets;
    std::vector<RHITextureSlot> textureSlots;
    std::vector<RHIMaterialParameter> parameters;
    std::vector<std::byte> pushConstantData;
};

struct RHIDrawCommand {
    RHIPipeline pipeline{};
    std::vector<RHIBindSet> bindSets;
    std::vector<RHIVertexStream> vertexStream;
    u32 vertexCount = 0;
    u32 instanceCount = 0;
    u32 firstVertex = 0;
    u32 firstInstance = 0;
};

struct RHIDrawIndexedCommand {
    RHIPipeline pipeline{};
    std::vector<RHIBindSet> bindSets;
    std::vector<RHIVertexStream> vertexStream;
    RHIIndexStream indexStream{};
    u32 vertexCount = 0;
    u32 instanceCount = 0;
    u32 firstVertex = 0;
    i32 vertexOffsetElements = 0;
    u32 firstInstance = 0;
};

struct RHIDispatchCommand {
    RHIPipeline pipeline{};
    std::vector<RHIBindSet> bindSets;
    u32 groupCountX = 1;
    u32 groupCountY = 1;
    u32 groupCountZ = 1;
};

struct RHIDrawIndirectCommand {
    RHIPipeline pipeline{};
    std::vector<RHIBindSet> bindSets;
    std::vector<RHIVertexStream> vertexStream;
    RHIBuffer argumentBuffer{};
    u64 argumentOffset = 0;
    u32 drawCount = 1;
    u32 stride = 0;
    RHIBuffer countBuffer{};
    u64 countBufferOffset = 0;
};

struct RHIDrawIndexedIndirectCommand {
    RHIPipeline pipeline{};
    std::vector<RHIBindSet> bindSets;
    std::vector<RHIVertexStream> vertexStream;
    RHIIndexStream indexStream{};
    RHIBuffer argementBuffer{};
    u64 argumentOffset = 0;
    u32 drawCount = 1;
    u32 stride = 0;
    RHIBuffer countBuffer{};
    u64 countBufferOffset = 0;
};

struct RHIDispatchIndirectCommand {
    RHIPipeline pipeline{};
    std::vector<RHIBindSet> bindSets;
    RHIBuffer argumentBuffer{};
    u64 argumentOffset = 0;
};

struct RHIDebugMarkerDesc {
    std::string name;
    std::array<float, 4> color{0.2F, 0.6F, 1.0F, 1.0F};
};

struct RHITimestampQueryCommand {
    RHIQueryPool queryPool{};
    u32 queryIndex = 0;
    RHIPipelineStage stage = RHIPipelineStage::BottomOfPipe;
};

struct RHIResetQueryCommand {
    RHIQueryPool queryPool{};
    u32 firstQuery = 0;
    u32 queryCount = 1;
};

struct RHIResolveQueryCommand {
    RHIQueryPool queryPool{};
    u32 firstQuery = 0;
    u32 queryCount = 1;
    RHIBuffer destination{};
    u64 destinationOffset = 0;
    u64 stride = sizeof(u64);
    bool waitForResults = true;
};

struct RHIRenderPassWorkload {
    std::string passName;
    RHIViewport viewport{};
    RHIRect2D scissor{};
    RHIResourceBarriers barriers;
    std::vector<RHIDebugMarkerDesc> debugMarkers;
    std::vector<RHIBufferCopyDesc> bufferCopies;
    std::vector<RHITextureCopyDesc> textureCopies;
    std::vector<RHIBufferTextureCopyDesc> bufferToTextureCopies;
    std::vector<RHIBufferTextureCopyDesc> textureToBufferCopies;
    std::vector<RHITextureBlitDesc> textureBlits;
    std::vector<RHIMipmapGenerationDesc> mipmapGenerations;
    std::vector<RHIResetQueryCommand> queryResets;
    std::vector<RHITimestampQueryCommand> timestampWrites;
    std::vector<RHIResolveQueryCommand> queryResolves;
    std::vector<RHIDrawCommand> draws;
    std::vector<RHIDrawIndexedCommand> indexedDraws;
    std::vector<RHIDrawIndirectCommand> indirectCommands;
    std::vector<RHIDrawIndexedIndirectCommand> indexedIndirectCommands;
    std::vector<RHIDispatchCommand> dispatches;
    std::vector<RHIDispatchIndirectCommand> indirectDispatches;
};

enum class RHIGPUWaitCPUSignalType : u8 {
    Binary,
    Timeline
};

struct RHIGPUWaitCPUSignalDesc {
    std::string debugName;
    RHIGPUWaitCPUSignalType type = RHIGPUWaitCPUSignalType::Binary;
    u64 initialValue = 0;
};

struct RHICPUWaitGPUSignalDesc {
    std::string debugName;
    bool signaled = false;
};

struct RHIQueueWaitDesc {
    RHIGPUWaitGPUSignal signal{};
    u64 value = 0;
    RHIPipelineStage stages = RHIPipelineStage::AllCommands;
};

struct RHIQueueSignalDesc {
    RHIGPUWaitGPUSignal signal{};
    u64 value = 0;
};

struct RHIQueueSubmitDesc {
    std::string debugName;
    RHIQueueType queue = RHIQueueType::Graphics;
    std::vector<std::string> passNames;
    std::vector<RHIQueueWaitDesc> waits;
    std::vector<RHIQueueSignalDesc> signals;
    RHICPUWaitGPUSignal cpuWaitGPUSignal{};
};

struct RHIPresentDesc {
    RHISwapchain swapchain{};
    u32 imageIndex = 0;
    std::vector<RHIGPUWaitGPUSignal> waitSignals;
    RHIPresentMode presentMode = RHIPresentMode::FIFO;
    bool allowTearing = false;
};

struct RHITransformData {
    float4x4 localToWorld{1.0F};
    float4x4 previousLocalToWorld{1.0F};
};

struct RHICameraData {
    float4x4 view{1.0F};
    float4x4 projection {1.0F};
    float4x4 viewProjection{1.0F};              ///< projection * view
    float4x4 previousViewProjection{1.0F};      ///< 上一帧 viewProjection，用于 motion vector/TAA。
    float3 position{0.0F};
    float nearPlane = 1.0F;
    float farPlane = 1000.0F;
    float verticalFovRadius = 1.0471975512F;    ///< 垂直视场角，单位弧度，默认约 60 度。
    float2 jitter{0.0F};                        ///< 当前帧投影抖动，TAA/TSR 常用。
    float2 previousJitter{0.0F};                ///< 上一帧投影抖动。
};

enum class RHILightType : u8 {
    Directional,
    Point,
    Spot
};

struct RHILightData {
    RHILightType type = RHILightType::Directional;
    float3 color{1.0F};
    float intensity = 1.0F;
    float3 direction{0.0F, -1.0F, 0.0F};
    float range = 10.F;
    float3 position{0.0F};
    float innerConeRadius = 0.0F;
    float outerConeRadius = 0.7853981634F;         ///< 聚光外锥角，默认约 45 度。
};

enum class RHIRenderQueue : u8 {
    Background,
    Opaque,
    AlphaTest,
    Transparent,
    Overlay
};

struct RHIRenderObjectDesc {
    std::string debugName;
    RHIMesh mesh{};
    RHIMaterial material{};
    RHITransformData transform{};
    u32 submeshIndex = 0;
    RHIRenderQueue queue = RHIRenderQueue::Opaque;
    u64 soringKey = 0;
    u32 layerMask = 0xFFFFFFFFu;
    RHIBoundingBox worldBounds{};
    RHIBoundingSphere worldBoundsSphere{};
    bool visible = true;
    bool castShadow = true;
    bool receiveShadow = true;
};

struct RHISceneEnviromentDesc {
    float3 ambientColor{0.03F};                     ///< 简单环境光颜色。
    float exposure = 1.0F;                          ///< 曝光倍率。
    RHITextureView skyTexture{};                    ///< 可选天空贴图。
    RHITextureView irradianceTexture{};             ///< 可选漫反射 IBL。
    RHITextureView prefilterReflectionTexture{};    ///< 可选预滤波反射 IBL。
    RHITextureView brdfLut{};                       ///< 可选 BRDF LUT。
};

struct RHIRenderCameraSetDesc {
    RHICameraData main{};
    std::vector<RHICameraData> additional;
};

struct RHIRenderLightSetDesc {
    std::vector<RHILightData> items;
};

struct RHIRenderObjectSetDesc {
    std::vector<RHIRenderObjectDesc> items;
};

enum class RHIRenderGraphResourceType : u8 {
    Buffer,
    Texture,
    SwapchainImage
};

enum class RHIRenderGraphResourceFlags : u32 {
    None = 0,                   ///< 无特殊 RenderGraph 行为，按普通内部资源处理。
    Imported = 1u << 0,         ///< 资源由外部创建并传入 graph，graph 不负责创建和销毁。
    Exported = 1u << 1,         ///< 资源结果需要在 graph 外继续使用，不能在最后一次内部读取后立即释放。
    Transient = 1u << 2,        ///< 资源只在 graph 内短期使用，可参与池化和生命周期压缩。
    AllowAliasing = 1u << 3,    ///< 允许与生命周期不重叠的其他资源共享底层内存。
    NeverCull = 1u << 4         ///< 即使看起来未被读取也不能裁剪，适合有调试、读回或外部副作用的资源。
};

template <>
struct RHIEnableEnumFlags<RHIRenderGraphResourceFlags> : std::true_type {};

[[nodiscard]] constexpr RHIRenderGraphResourceFlags operator|(RHIRenderGraphResourceFlags lhs, RHIRenderGraphResourceFlags rhs) noexcept {
    return RHIEnumBitOr(lhs, rhs);
}

[[nodiscard]] constexpr RHIRenderGraphResourceFlags operator&(RHIRenderGraphResourceFlags lhs, RHIRenderGraphResourceFlags rhs) noexcept {
    return RHIEnumBitAnd(lhs, rhs);
}

constexpr RHIRenderGraphResourceFlags& operator|=(RHIRenderGraphResourceFlags& lhs, RHIRenderGraphResourceFlags rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

/// pass 对某个 graph 资源的读写引用。
struct RHIRenderGraphResourceRef {
    std::string name;
    RHIRenderGraphResourceType type = RHIRenderGraphResourceType::Texture;
    RHIResourceState state = RHIResourceState::ShaderRead;
    RHIPipelineStage stages = RHIPipelineStage::AllCommands;
    RHIAccessFlags access = RHIAccessFlags::None;
};

struct RHIRenderGraphBufferDesc {
    std::string name;
    RHIBufferDesc desc{};
    RHIRenderGraphResourceFlags flags = RHIRenderGraphResourceFlags::None;
    bool imported = false;
    RHIBuffer externalHandle{};
};

struct RHIRenderGraphTextureDesc {
    std::string name;
    RHITextureDesc desc{};
    RHIRenderGraphResourceFlags flags = RHIRenderGraphResourceFlags::None;
    bool imported = false;
    RHITexture externalHandle{};
};

struct RHIRenderGraphAttachmentDesc {
    std::string resourceName;
    RHITextureAspect aspect = RHITextureAspect::Color;
    u32 mipLevel = 0;
    u32 arrayLayer = 0;
    RHILoadOp loadOp = RHILoadOp::Clear;
    RHIStoreOp storeOp = RHIStoreOp::Store;
    RHIClearValue clearValue{};
};

enum class RHIRenderGraphPassType : u8 {
    Raster,
    Compute,
    Copy,
    Present
};

struct RHIRenderGraphPassDesc {
    std::string name;                                                   ///< pass 名称，需和 RHIRenderPassWorkload::passName 对应。
    RHIRenderGraphPassType type = RHIRenderGraphPassType::Raster;       ///< pass 类型。
    RHIQueueType queue = RHIQueueType::Graphics;                        ///< pass 希望运行在哪类队列。
    std::vector<std::string> dependOnPasses;                            ///< 仅用于无法从资源读写推导的显式先决 pass；通常应优先声明资源依赖。
    std::vector<RHIRenderGraphResourceRef> reads;                       ///< pass 读取的资源及访问状态。
    std::vector<RHIRenderGraphResourceRef> writes;                      ///< pass 写入的资源及访问状态。
    std::vector<RHIRenderGraphAttachmentDesc> colorAttachments;         ///< color attachments。
    std::optional<RHIRenderGraphAttachmentDesc> depthStencilAttachment; ///< 可选 depth-stencil attachment。
    bool allowAsyncCompute = false;                                     ///< compute pass 是否允许异步调度，后端需验证队列和同步支持。
    bool cullable = true;                                               ///< 如果 pass 结果未被使用，RenderGraph 是否允许裁剪该 pass。
    bool hasSideEffect = false;                                         ///< true 表示 pass 有外部副作用，即使输出未被读取也不能裁剪。
};

struct RHIRenderGraphDesc {
    std::vector<RHIRenderGraphBufferDesc> buffers;
    std::vector<RHIRenderGraphTextureDesc> textures;
    std::vector<RHIRenderGraphPassDesc> passes;
};

struct RHIFrameRenderSettings {
    RHIExtent2D drawableSize{};
    RHIViewport viewport{};
    RHIRect2D scissor{};
    u64 frameIndex = 0;
    float deltaTimeSeconds = 0.0F;
    u32 maxFrameInFlight = 2;
    bool enableVsync = true;
    bool enableHdr = false;
};

struct RHIFramePacket {
    RHIFrameRenderSettings settings{};              ///< 当前帧设置。
    RHISwapchainDesc swapchain{};                   ///< 当前帧目标 swapchain 需求。
    RHIUploadBatchDesc upload{};                    ///< 本帧开始前需要执行的资源上传。
    RHIRenderCameraSetDesc cameras{};               ///< 相机输入，和物体/光源解耦。
    RHISceneEnviromentDesc enviroment{};            ///< 场景环境和后处理基础参数。
    RHIRenderLightSetDesc lights{};                 ///< 光源输入，和物体/相机解耦。
    RHIRenderObjectSetDesc objects{};               ///< 可渲染物体输入，和相机/光源解耦。
    RHIRenderGraphDesc graph{};                     ///< pass 和资源依赖图。
    std::vector<RHIRenderPassWorkload> workloads;   ///< 每个 pass 的具体 draw/dispatch 命令。
    std::vector<RHIQueueSubmitDesc> submissions;    ///< 队列提交计划；为空时后端可按 graph 自动生成。
    std::optional<RHIPresentDesc> present;          ///< 可选呈现请求，离屏渲染帧可以为空。
};

struct RHICapabilities {
    RHIGraphicsAPI api = RHIGraphicsAPI::Unknown;
    std::string adapterName;
    u64 dedicatedVideoMemory = 0;
    u64 sharedSystemMemory = 0;
    RHIRenderFeature features = RHIRenderFeature::None;
    u32 maxTexture2DSize = 0;
    u32 maxTexture3DSize = 0;
    u32 maxTextureCubeSize = 0;
    u32 maxTextureArrayLayers = 0;
    u32 maxColorAttachments = 0;
    u32 maxBindSets = 0;
    u32 maxBindingPerGroup = 0;
    u32 maxVertexBuffers = 0;
    u32 maxVertexAttributes = 0;
    u32 maxPushConstantSize = 0;
    u64 minUniformBufferOffsetAlignment = 0;
    u64 minStorageBufferOffsetAlignment = 0;
    u64 optionalBufferCopyOffsetAlignment = 0;
    u64 optionalBufferCopyRowPitchAlignment = 0;
    RHISampleCount maxSampleCount = RHISampleCount::Count1;
    float maxSamplerAnisotropy = 1.0F;
    b8 supportsCompute = false;
    b8 supportsGeometryShader = false;
    b8 supportsTessellation = false;
    b8 supportsMeshShader = false;
    b8 supportsRayTracing = false;
    b8 supportsBindless = false;
    b8 supportsSamplerAnisotropy = false;
    b8 supportsSamplerCompare = false;
    b8 supportsTimestampQuery = false;
    b8 supportsOcclusionQuery = false;
    b8 supportsPipelineStatisticsQuery = false;
    b8 supportsIndirectDraw = false;
    b8 supportsDrawIndirectCount = false;
    b8 supportsDynamicRendering = false;
    b8 supportsDebugMarker = false;
    b8 supportsTextureCompressionBC = false;
    b8 supportsTextureCompressionETC2 = false;
    b8 supportsTextureCompressionASTC = false;
};

} // namespace RHI