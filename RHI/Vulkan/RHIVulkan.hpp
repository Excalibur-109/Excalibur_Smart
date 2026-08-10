#pragma once

#include "../RHIDefinitions.hpp"
#include "../RenderGraph/RHIRenderGraph.hpp"

#include <vulkan/vulkan.h>

#include <memory>
#include <functional>
#include <string>
#include <vector>

namespace RHI {

struct RHIVulkanSurfaceDesc {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    std::function<VkSurfaceKHR(VkInstance)> createSurface;
    bool ownsSurface = false;
};

struct RHIVulkanDesc {
    RHIBackendDesc backend{};
    RHIVulkanSurfaceDesc surface{};
    std::vector<const char*> requiredInstanceExtensions;
    std::vector<const char*> optionalInstanceExtensions;
    std::vector<const char*> requiredDeviceExtensions;
    std::vector<const char*> optionalDeviceExtensions;
    std::vector<RHIQueueDesc> queues;
};

struct RHIVulkanNativeHandles {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue computeQueue = VK_NULL_HANDLE;
    VkQueue transferQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    u32 graphicsQueueFamily = RHI_INVALID_INDEX;
    u32 computeQueueFamily = RHI_INVALID_INDEX;
    u32 transferQueueFamily = RHI_INVALID_INDEX;
    u32 presentQueueFamily = RHI_INVALID_INDEX;
};

class RHIVulkan {
    RHIVulkan();
    ~RHIVulkan();

    RHIVulkan(const RHIVulkan&) = delete;
    RHIVulkan& operator=(const RHIVulkan&) = delete;
    RHIVulkan(RHIVulkan&&) noexcept;
    RHIVulkan& operator=(RHIVulkan&&) noexcept;

    [[nodiscard]] bool Initialize(const RHIVulkanDesc& desc, std::string* errorMessage = nullptr);
    void Shutdown() noexcept;
    [[nodiscard]] bool IsInitialized() const noexcept;
    [[nodiscard]] const RHICapabilities& Capabilities() const noexcept;
    [[nodiscard]] const RHIVulkanNativeHandles& NativeHandles() const noexcept;
    [[nodiscard]] RHIBuffer CreateBuffer(const RHIBufferDesc& desc);
    [[nodiscard]] RHITexture CreateTexture(const RHITextureDesc& desc);
    [[nodiscard]] RHITextureView CreateTextureView(const RHITextureViewDesc& desc);
    [[nodiscard]] RHISampler CreateSampler(const RHISamplerDesc& desc);
    [[nodiscard]] RHIShader CreateShaderModule(const RHIShaderDesc& desc);
    [[nodiscard]] RHIBindSetLayout CreateBindSetLayout(const RHIBindSetLayoutDesc& desc);
    [[nodiscard]] RHIBindSet CreateBindSet(const RHIBindSetDesc& desc);
    [[nodiscard]] RHIPipelineLayout CreatePipelineLayout(const RHIPipelineLayoutDesc& desc);
    [[nodiscard]] RHIPipelineCache CreatePipelineCache(const RHIPipelineCacheDesc& desc);
    [[nodiscard]] RHIPipeline CreateGraphicsPipeline(const RHIGraphicsPipelineDesc& desc);
    [[nodiscard]] RHIPipeline CreateComputePipeline(const RHIComputePipelineDesc& desc);
    [[nodiscard]] RHIQueryPool CreateQueryPool(const RHIQueryPoolDesc& desc);
    [[nodiscard]] RHIGPUWaitGPUSignal CreateGPUWaitGPUSignal(const RHIGPUWaitGPUSignalDesc& desc);
    [[nodiscard]] RHICPUWaitGPUSignal CreateCPUWaitGPUSignal(const RHICPUWaitGPUSignalDesc& desc);
    [[nodiscard]] RHISwapchain CreateSwapchain(const RHISwapchainDesc& desc);
    [[nodiscard]] std::vector<RHITexture> GetSwapchainImages(RHISwapchain handle) const;
    [[nodiscard]] std::vector<RHITextureView> GetSwapchainImageViews(RHISwapchain handle) const;
    [[nodiscard]] RHIFormat GetSwapchainFormat(RHISwapchain handle) const;
    [[nodiscard]] RHIExtent2D GetSwapchainExtent(RHISwapchain handle) const;
    [[nodiscard]] bool AcquireNextImage(RHISwapchain swapchain, RHIGPUWaitGPUSignal gpuWaitGPUSignal, RHICPUWaitGPUSignal cpuWaitGPUSignal, u32* imageIndex, std::string* errorMessage = nullptr);
    [[nodiscard]] bool Submit(const RHIQueueSubmitDesc& desc, std::string* errorMessage = nullptr);
    [[nodiscard]] bool Present(const RHIPresentDesc& desc, std::string* errorMessage = nullptr);
    [[nodiscard]] bool SubmitFrame(const RHIFramePacket& packet, std::string* errorMessage = nullptr);
    [[nodiscard]] bool SubmitFrame(const RHIFramePacket& packet, const RHIRenderGraphExecutionPlan& graphPlan, std::string* errorMessage = nullptr);
    void WaitIdle() const noexcept;

    void Destroy(RHIBuffer handle) noexcept;
    void Destroy(RHITexture handle) noexcept;
    void Destroy(RHITextureView handle) noexcept;
    void Destroy(RHISampler handle) noexcept;
    void Destroy(RHIShader handle) noexcept;
    void Destroy(RHIBindSetLayout handle) noexcept;
    void Destroy(RHIBindSet handle) noexcept;
    void Destroy(RHIPipelineLayout handle) noexcept;
    void Destroy(RHIPipelineCache handle) noexcept;
    void Destroy(RHIPipeline handle) noexcept;
    void Destroy(RHIQueryPool handle) noexcept;
    void Destroy(RHIGPUWaitGPUSignal handle) noexcept;
    void Destroy(RHICPUWaitGPUSignal handle) noexcept;
    void Destroy(RHISwapchain handle) noexcept;

private:
    [[nodiscard]] bool RecordAndSubmitFrame(const RHIFramePacket& packet, const RHIRenderGraphExecutionPlan& graphPlan, std::string* errorMessage);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};
    
} // namespace RHI