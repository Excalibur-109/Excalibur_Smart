#pragma once

#include "../RHIDefinitions.hpp"

#include <span>
#include <string>
#include <vector>

namespace RHI {

struct RHIRenderGraphResourceId {
    RHIRenderGraphResourceType type = RHIRenderGraphResourceType::Texture;
    u32 index = RHI_INVALID_INDEX;
    [[nodiscard]] constexpr b8 IsBuffer() const noexcept {
        return type == RHIRenderGraphResourceType::Buffer;
    }
    [[nodiscard]] constexpr b8 IsTexture() const noexcept {
        return !IsBuffer();
    }
};

struct RHIRenderGraphTransition {
    RHIRenderGraphResourceId resource{};
    RHIResourceState         before            = RHIResourceState::Undefined;
    RHIResourceState         after             = RHIResourceState::Common;
    RHIPipelineStage         sourceStages      = RHIPipelineStage::TopOfPipe;
    RHIPipelineStage         destinationStages = RHIPipelineStage::AllCommands;
    RHIAccessFlags           sourceAccess      = RHIAccessFlags::None;
    RHIAccessFlags           destinationAccess = RHIAccessFlags::None;
    RHIQueueType             sourceQueue       = RHIQueueType::Graphics;
    RHIQueueType             destinationQueue  = RHIQueueType::Graphics;
    b8 discardContents = false;
    b8 aliasingBarrier = false;
};

struct RHICompiledRenderGraphAttachment {
    u32 textureIndex    = RHI_INVALID_INDEX;
    u32 attachmentIndex = RHI_INVALID_INDEX;
};

struct RHICompiledRenderGraphPass {
    u32 sourcePassIndex = RHI_INVALID_INDEX;
    u32 workloadIndex = RHI_INVALID_INDEX;
    RHIQueueType queue = RHIQueueType::Graphics;
    std::vector<u32> dependencies;
    std::vector<RHIRenderGraphTransition> transitions;
    std::vector<RHICompiledRenderGraphAttachment> colorAttachments;
    std::optional<RHICompiledRenderGraphAttachment> depthStencilAttachment;
};

struct RHIRenderGraphResourceLifetime {
    u32 firstPass = RHI_INVALID_INDEX;
    u32 lastPass  = RHI_INVALID_INDEX;
};

struct RHIRenderGraphExecutionPlan {
    u64 structureHash = 0;
    std::vector<RHICompiledRenderGraphPass> passes;
    std::vector<RHIRenderGraphResourceLifetime> bufferLifetimes;
    std::vector<RHIRenderGraphResourceLifetime> textureLifetimes;
    std::vector<u32> bufferAllocationSlots;
    std::vector<u32> textureAllocationSlots;
    u32 bufferAllocationCount = 0;
    u32 textureAllocationCount = 0;
    std::vector<b8> culledPasses;
};

struct RHIRenderGraphCompileResult {
    RHIRenderGraphExecutionPlan plan;
    std::vector<std::string> errors;

    [[nodiscard]] b8 Succeeded() const noexcept { return errors.empty(); }
    [[nodiscard]] std::string ErrorMessage() const;
};

[[nodiscard]] RHIRenderGraphCompileResult CompileRHIRenderGraph(
    const RHIRenderGraphDesc& graph,
    std::span<const RHIRenderPassWorkload> workloads = {}
);

[[nodiscard]] u64 HashRHIRenderGraphStructure(
    const RHIRenderGraphDesc& graph,
    std::span<const RHIRenderPassWorkload> workloads = {}
) noexcept;

[[nodiscard]] b8 ValidateRHIRenderGraphSubmissions(
    const RHIRenderGraphDesc& graph,
    const RHIRenderGraphExecutionPlan& plan,
    std::span<const RHIQueueSubmitDesc> submissions,
    std::string* errorMessage = nullptr
);

} // namespace RHI