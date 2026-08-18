#pragma once

#include "RHI/RHIDefinitions.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// 这个类位于 Demo 层而不是 RHI 核心：RHI 负责执行任意 RenderGraph，
// ForwardRenderPipeline 只约定一个常见的前向渲染帧应如何组织阶段。
// 因此它不包含 Vk*/ID3D* 类型，也不拥有 GPU 资源，能直接用于全部 RHI 后端。
namespace rhi::pipeline {

enum class ForwardStage : u32 {
    Shadow = 0,    // 从光源视角写入 shadow map。
    Opaque,        // 主相机的不透明几何、天空盒和光照。
    PostProcess,   // 可选：tone mapping、bloom、TAA 等屏幕空间阶段。
    Overlay,       // 可选：UI、debug geometry、gizmo。
    Present,       // 将最终 swapchain image 声明为 Present 状态。
    Count
};

/// RenderGraph pass 的通用前向阶段编排器。
///
/// 场景代码负责声明资源读写和创建 draw workload；该类负责把它们以稳定的
/// Shadow -> Opaque -> PostProcess -> Overlay -> Present 顺序写入一个 FramePacket。
/// Resource dependency 仍由 RenderGraph 编译器推导，所以后处理阶段可以换成任意
/// 自定义输入/输出纹理，而无需在这个类中添加后端分支。
class ForwardRenderPipeline {
public:
    /// 每个阶段最多一个 pass。传入的 name 会原样保留，用于 RenderGraph 和 queue submit。
    void SetPass(ForwardStage stage, RHI::RHIRenderGraphPassDesc pass) {
        StageData& data = Stage(stage);
        if (data.pass.has_value()) {
            throw std::runtime_error("ForwardRenderPipeline stage already has a pass");
        }
        if (pass.name.empty()) {
            throw std::runtime_error("ForwardRenderPipeline pass name must not be empty");
        }
        if (stage == ForwardStage::Present &&
            pass.type != RHI::RHIRenderGraphPassType::Present) {
            throw std::runtime_error("ForwardRenderPipeline Present stage requires a Present pass");
        }
        if (stage != ForwardStage::Present &&
            pass.type == RHI::RHIRenderGraphPassType::Present) {
            throw std::runtime_error("Only the Present stage may contain a Present pass");
        }
        data.pass = std::move(pass);
    }

    /// workload 的 passName 必须和同阶段 pass 相同，避免 draw 被录制到错误的 attachment。
    void SetWorkload(ForwardStage stage, RHI::RHIRenderPassWorkload workload) {
        StageData& data = Stage(stage);
        if (data.workload.has_value()) {
            throw std::runtime_error("ForwardRenderPipeline stage already has a workload");
        }
        data.workload = std::move(workload);
    }

    /// 将阶段写入 packet。Opaque 和 Present 是最小的可呈现前向帧；其余阶段可选。
    /// Commit 后对象不应再复用，因为 pass/workload 所有权已经移动给 FramePacket。
    void Commit(RHI::RHIFramePacket& packet) {
        if (committed_) {
            throw std::runtime_error("ForwardRenderPipeline was already committed");
        }
        ValidateRequiredStages();

        for (StageData& data : stages_) {
            if (!data.pass.has_value()) {
                if (data.workload.has_value()) {
                    throw std::runtime_error("ForwardRenderPipeline workload has no matching pass");
                }
                continue;
            }

            if (data.workload.has_value() &&
                data.workload->passName != data.pass->name) {
                throw std::runtime_error("ForwardRenderPipeline workload passName does not match its pass");
            }

            passNames_.push_back(data.pass->name);
            packet.graph.passes.push_back(std::move(*data.pass));
            if (data.workload.has_value()) {
                packet.workloads.push_back(std::move(*data.workload));
            }
        }
        committed_ = true;
    }

    /// 直接提供给 RHIQueueSubmitDesc::passNames，保持图描述与实际 queue submission 同序。
    [[nodiscard]] const std::vector<std::string>& PassNames() const noexcept {
        return passNames_;
    }

private:
    struct StageData {
        std::optional<RHI::RHIRenderGraphPassDesc> pass;
        std::optional<RHI::RHIRenderPassWorkload> workload;
    };

    [[nodiscard]] static constexpr size_t ToIndex(ForwardStage stage) noexcept {
        return static_cast<size_t>(stage);
    }

    [[nodiscard]] StageData& Stage(ForwardStage stage) {
        const size_t index = ToIndex(stage);
        if (index >= stages_.size()) {
            throw std::runtime_error("ForwardRenderPipeline stage is invalid");
        }
        return stages_[index];
    }

    void ValidateRequiredStages() const {
        if (!stages_[ToIndex(ForwardStage::Opaque)].pass.has_value()) {
            throw std::runtime_error("ForwardRenderPipeline requires an Opaque pass");
        }
        if (!stages_[ToIndex(ForwardStage::Present)].pass.has_value()) {
            throw std::runtime_error("ForwardRenderPipeline requires a Present pass");
        }
    }

    std::array<StageData, static_cast<size_t>(ForwardStage::Count)> stages_{};
    std::vector<std::string> passNames_;
    bool committed_ = false;
};

} // namespace rhi::pipeline
