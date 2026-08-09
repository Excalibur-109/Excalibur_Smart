#pragma once

#include "RHIDeviceDesc.hpp"

#include <memory>
#include <string>
#include <vector>

namespace RHI {

class RHIDevice {

public:
    explicit RHIDevice(RHIGraphicsAPI requiredApi = RHIGraphicsAPI::Unknown);
    ~RHIDevice();

    RHIDevice(const RHIDevice&) = delete;
    RHIDevice& operator=(const RHIDevice&) = delete;
    RHIDevice(RHIDevice&&) noexcept;
    RHIDevice& operator=(RHIDevice&&) noexcept;

    [[nodiscard]] RHIGraphicsAPI Api() const noexcept;
    [[nodiscard]] const char* BackendNamme() const noexcept;
    [[nodiscard]] b8 Initialize(const RHIDeviceCreateDesc& desc, std::string* errorMessage = nullptr);
    void Shutdown() noexcept;
    [[nodiscard]] b8 IsInitialized() const noexcept;
    [[nodiscard]] const RHICapabilities& Capabilities() const noexcept;

    [[nodiscard]] RHIBuffer         CreateBuffer            (const RHIBufferDesc& desc);
    [[nodiscard]] RHITexture        CreateTexture           (const RHITextureDesc& desc);
    [[nodiscard]] RHITextureView    CreateTextureView       (const RHITextureViewDesc& desc);
    [[nodiscard]] RHISampler        CreateSampler           (const RHISamplerDesc& desc);
    [[nodiscard]] RHIShader         CreateShaderModule      (const RHIShaderDesc& desc);
    [[nodiscard]] RHIBindSetLayout  CreateBindSetLayout     (const RHIBindSetLayoutDesc& desc);
    [[nodiscard]] RHIBindSet        CreateBindSet           (const RHIBindSetDesc& desc);
    [[nodiscard]] RHIPipelineLayout CreatePipelineLayout    (const RHIPipelineLayoutDesc& desc);
    [[nodiscard]] RHIPipelineCache  CreatePipelineCache     (const RHIPipelineCache& desc);
    [[nodiscard]] RHIPipeline       CreateGraphicsPipeline  (const RHIGraphicsPipelineDesc& desc);
    [[nodiscard]] RHIPipeline       CreateComputePipeline   (const RHIComputePipelineDesc& desc);
    [[nodiscard]] RHISwapchain      CreateSwapchain         (const RHISwapchainDesc& desc);
    [[nodiscard]] RHIQueryPool      CreateQueryPool         (const RHIQueryPoolDesc& desc);

    [[nodiscard]] RHIGPUWaitGPUSignal   CreateGPUWaitGPUSignal  (const RHIGPUWaitGPUSignalDesc& desc);
    [[nodiscard]] RHICPUWaitGPUSignal   CreateCPUWaitGPUSignal  (const RHICPUWaitGPUSignalDesc& desc);

    [[nodiscard]] std::vector<RHITexture>       GetSwapchainImages      (RHISwapchain handle) const;
    [[nodiscard]] std::vector<RHITextureView>   GetSwapchainImageViews  (RHISwapchain handle) const;
    [[nodiscard]] RHIFormat                     GetSwapchainFormat      (RHISwapchain handle) const;
    [[nodiscard]] RHIExtent2D                   GetSwapchainExtent      (RHISwapchain handle) const;

    [[nodiscard]] b8 AcquireNextImage(
        RHISwapchain swapchain,
        RHIGPUWaitGPUSignal gpuWaitGPUSignal,
        RHICPUWaitGPUSignal cpuWaitGPUSignal,
        u32* imageIndex,
        std::string* errorMessage = nullptr
    );

    [[nodiscard]] b8 Submit     (const RHIQueueSubmitDesc& desc, std::string* errorMessage = nullptr);
    [[nodiscard]] b8 Present    (const RHIPresentDesc& desc, std::string* errorMessage = nullptr);
    [[nodiscard]] b8 SubmitFrame(const RHIFramePacket& packet, std::string* errorMessage = nullptr);

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
    struct Impl;
    std::unique_ptr<Impl> impl_;

};

} // namespace RHI