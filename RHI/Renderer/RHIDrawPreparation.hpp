#pragma once

#include "RHI/RHIDefinitions.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <future>
#include <iterator>
#include <limits>
#include <span>
#include <thread>
#include <utility>
#include <vector>

// This is intentionally an engine-layer helper rather than a core RHI module.
// It prepares draw commands before they are placed in a RenderGraph workload and
// therefore stays independent of Vulkan, D3D11, and D3D12 command recording.
namespace rhi::renderer {

/// A plane uses the signed-distance form dot(normal, point) + distance = 0.
struct RHIFrustumPlane {
    float3 normal{0.0F};
    float distance = 0.0F;
};

/// Six inward-facing world-space planes used for inexpensive sphere culling.
struct RHIFrustum {
    std::array<RHIFrustumPlane, 6> planes{};

    [[nodiscard]] bool Intersects(const RHI::RHIBoundingSphere& sphere) const noexcept {
        const float radius = std::max(0.0F, sphere.radius);
        for (const RHIFrustumPlane& plane : planes) {
            const float signedDistance =
                plane.normal.x * sphere.center.x +
                plane.normal.y * sphere.center.y +
                plane.normal.z * sphere.center.z +
                plane.distance;
            if (signedDistance < -radius) {
                return false;
            }
        }
        return true;
    }
};

/// Caller supplied keys. The preparation system does not guess material identity
/// from descriptor contents, so different material systems can choose their own IDs.
struct RHIIndexedDrawSortKey {
    u64 renderLayer = 0;
    u64 material = 0;
    u64 mesh = 0;
    float depth = 0.0F;
};

/// One scene-level indexed draw before visibility, sorting, and optional instance merging.
struct RHIIndexedDrawItem {
    RHI::RHIDrawIndexedCommand command{};
    RHI::RHIBoundingSphere bounds{};
    RHIIndexedDrawSortKey sortKey{};
    bool hasBounds = false;

    // Opt-in only. The caller must provide a vertex shader and instance-data buffer
    // that use firstInstance / instanceCount to select the correct per-instance data.
    bool allowInstanceMerge = false;
};

/// Controls CPU-side preparation. Small workloads stay serial to avoid worker overhead.
struct RHIDrawPreparationOptions {
    const RHIFrustum* frustum = nullptr;
    u32 workerCount = 0;               // Zero selects hardware_concurrency().
    u32 parallelThreshold = 128;       // Minimum source item count before launching workers.
};

/// A contiguous range of emitted draws that shares pipeline/material/mesh sort state.
struct RHIIndexedDrawBatch {
    u32 firstDraw = 0;
    u32 drawCount = 0;
    u64 renderLayer = 0;
    u64 pipeline = 0;
    u64 material = 0;
    u64 mesh = 0;
};

struct RHIDrawPreparationStats {
    u32 inputDrawCount = 0;
    u32 visibleDrawCount = 0;
    u32 culledDrawCount = 0;
    u32 emittedDrawCount = 0;
    u32 mergedDrawCount = 0;
    u32 stateBatchCount = 0;
};

struct RHIPreparedIndexedDraws {
    std::vector<RHI::RHIDrawIndexedCommand> draws;
    std::vector<RHIIndexedDrawBatch> batches;
    RHIDrawPreparationStats stats{};
};

namespace detail {

struct IndexedDrawCandidate {
    RHI::RHIDrawIndexedCommand command{};
    RHIIndexedDrawSortKey sortKey{};
    bool allowInstanceMerge = false;
};

struct LocalCollection {
    std::vector<IndexedDrawCandidate> visible;
    u32 culledCount = 0;
};

[[nodiscard]] inline float NormalizedDepth(float value) noexcept {
    return std::isfinite(value) ? value : 0.0F;
}

[[nodiscard]] inline bool LessOpaque(const IndexedDrawCandidate& lhs,
                                     const IndexedDrawCandidate& rhs) noexcept {
    if (lhs.sortKey.renderLayer != rhs.sortKey.renderLayer) {
        return lhs.sortKey.renderLayer < rhs.sortKey.renderLayer;
    }
    if (lhs.command.pipeline.value != rhs.command.pipeline.value) {
        return lhs.command.pipeline.value < rhs.command.pipeline.value;
    }
    if (lhs.sortKey.material != rhs.sortKey.material) {
        return lhs.sortKey.material < rhs.sortKey.material;
    }
    if (lhs.sortKey.mesh != rhs.sortKey.mesh) {
        return lhs.sortKey.mesh < rhs.sortKey.mesh;
    }

    const float lhsDepth = NormalizedDepth(lhs.sortKey.depth);
    const float rhsDepth = NormalizedDepth(rhs.sortKey.depth);
    if (lhsDepth != rhsDepth) {
        return lhsDepth < rhsDepth;
    }
    return lhs.command.firstInstance < rhs.command.firstInstance;
}

[[nodiscard]] inline bool SameVertexStreams(
    const std::vector<RHI::RHIVertexStream>& lhs,
    const std::vector<RHI::RHIVertexStream>& rhs) noexcept {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t index = 0; index < lhs.size(); ++index) {
        const RHI::RHIVertexStream& left = lhs[index];
        const RHI::RHIVertexStream& right = rhs[index];
        if (left.buffer != right.buffer || left.binding != right.binding ||
            left.offset != right.offset || left.stride != right.stride) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] inline bool SameBindSets(const std::vector<RHI::RHIBindSet>& lhs,
                                       const std::vector<RHI::RHIBindSet>& rhs) noexcept {
    return lhs == rhs;
}

[[nodiscard]] inline bool SameIndexedDrawState(
    const RHI::RHIDrawIndexedCommand& lhs,
    const RHI::RHIDrawIndexedCommand& rhs) noexcept {
    return lhs.pipeline == rhs.pipeline &&
           SameBindSets(lhs.bindSets, rhs.bindSets) &&
           SameVertexStreams(lhs.vertexStreams, rhs.vertexStreams) &&
           lhs.indexStream.buffer == rhs.indexStream.buffer &&
           lhs.indexStream.indexType == rhs.indexStream.indexType &&
           lhs.indexStream.offset == rhs.indexStream.offset &&
           lhs.indexStream.indexCount == rhs.indexStream.indexCount &&
           lhs.indexCount == rhs.indexCount &&
           lhs.firstIndex == rhs.firstIndex &&
           lhs.vertexOffsetElements == rhs.vertexOffsetElements;
}

[[nodiscard]] inline bool CanMergeInstances(const IndexedDrawCandidate& previous,
                                            const IndexedDrawCandidate& next) noexcept {
    if (!previous.allowInstanceMerge || !next.allowInstanceMerge ||
        previous.sortKey.renderLayer != next.sortKey.renderLayer ||
        previous.sortKey.material != next.sortKey.material ||
        previous.sortKey.mesh != next.sortKey.mesh ||
        !SameIndexedDrawState(previous.command, next.command)) {
        return false;
    }

    const u64 previousEnd = static_cast<u64>(previous.command.firstInstance) +
                            static_cast<u64>(previous.command.instanceCount);
    return previousEnd == static_cast<u64>(next.command.firstInstance) &&
           static_cast<u64>(previous.command.instanceCount) +
                   static_cast<u64>(next.command.instanceCount) <=
               static_cast<u64>(std::numeric_limits<u32>::max());
}

[[nodiscard]] inline LocalCollection CollectRange(
    std::span<const RHIIndexedDrawItem> items,
    std::size_t first,
    std::size_t last,
    const RHIFrustum* frustum) {
    LocalCollection result{};
    result.visible.reserve(last - first);
    for (std::size_t index = first; index < last; ++index) {
        const RHIIndexedDrawItem& item = items[index];
        if (frustum != nullptr && item.hasBounds && !frustum->Intersects(item.bounds)) {
            ++result.culledCount;
            continue;
        }
        result.visible.push_back({item.command, item.sortKey, item.allowInstanceMerge});
    }
    return result;
}

} // namespace detail

/// Prepare opaque indexed draws. It preserves renderLayer ordering, sorts each layer by
/// pipeline/material/mesh/depth, and merges only explicitly instance-compatible commands.
[[nodiscard]] inline RHIPreparedIndexedDraws PrepareOpaqueIndexedDraws(
    std::span<const RHIIndexedDrawItem> items,
    const RHIDrawPreparationOptions& options = {}) {
    RHIPreparedIndexedDraws result{};
    result.stats.inputDrawCount = static_cast<u32>(std::min<std::size_t>(
        items.size(), static_cast<std::size_t>(std::numeric_limits<u32>::max())));
    if (items.empty()) {
        return result;
    }

    const u32 hardwareThreads = std::max(1U, std::thread::hardware_concurrency());
    const u32 desiredWorkers = options.workerCount == 0 ? hardwareThreads : options.workerCount;
    const std::size_t workerCount =
        items.size() >= options.parallelThreshold
            ? std::min<std::size_t>(std::max(1U, desiredWorkers), items.size())
            : 1U;

    std::vector<detail::IndexedDrawCandidate> candidates;
    candidates.reserve(items.size());
    if (workerCount == 1U) {
        detail::LocalCollection collected =
            detail::CollectRange(items, 0, items.size(), options.frustum);
        result.stats.culledDrawCount = collected.culledCount;
        candidates = std::move(collected.visible);
    } else {
        std::vector<std::future<detail::LocalCollection>> futures;
        futures.reserve(workerCount);
        const std::size_t baseChunkSize = items.size() / workerCount;
        const std::size_t remainder = items.size() % workerCount;
        std::size_t first = 0;
        for (std::size_t worker = 0; worker < workerCount; ++worker) {
            const std::size_t count = baseChunkSize + (worker < remainder ? 1U : 0U);
            const std::size_t last = first + count;
            futures.push_back(std::async(
                std::launch::async,
                [items, first, last, frustum = options.frustum]() {
                    return detail::CollectRange(items, first, last, frustum);
                }));
            first = last;
        }
        for (std::future<detail::LocalCollection>& future : futures) {
            detail::LocalCollection collected = future.get();
            result.stats.culledDrawCount += collected.culledCount;
            candidates.insert(
                candidates.end(),
                std::make_move_iterator(collected.visible.begin()),
                std::make_move_iterator(collected.visible.end()));
        }
    }

    result.stats.visibleDrawCount = static_cast<u32>(candidates.size());
    std::stable_sort(candidates.begin(), candidates.end(), detail::LessOpaque);

    result.draws.reserve(candidates.size());
    result.batches.reserve(candidates.size());
    std::vector<bool> emittedCanMerge;
    emittedCanMerge.reserve(candidates.size());
    std::vector<RHIIndexedDrawSortKey> emittedSortKeys;
    emittedSortKeys.reserve(candidates.size());

    for (detail::IndexedDrawCandidate& candidate : candidates) {
        if (!result.draws.empty()) {
            detail::IndexedDrawCandidate previous{};
            previous.command = result.draws.back();
            previous.sortKey = emittedSortKeys.back();
            previous.allowInstanceMerge = emittedCanMerge.back();
            if (detail::CanMergeInstances(previous, candidate)) {
                result.draws.back().instanceCount += candidate.command.instanceCount;
                ++result.stats.mergedDrawCount;
                continue;
            }
        }

        const bool startsNewBatch = result.batches.empty() ||
            result.batches.back().renderLayer != candidate.sortKey.renderLayer ||
            result.batches.back().pipeline != candidate.command.pipeline.value ||
            result.batches.back().material != candidate.sortKey.material ||
            result.batches.back().mesh != candidate.sortKey.mesh;
        if (startsNewBatch) {
            result.batches.push_back({
                static_cast<u32>(result.draws.size()),
                0,
                candidate.sortKey.renderLayer,
                candidate.command.pipeline.value,
                candidate.sortKey.material,
                candidate.sortKey.mesh});
        }

        result.draws.push_back(std::move(candidate.command));
        emittedCanMerge.push_back(candidate.allowInstanceMerge);
        emittedSortKeys.push_back(candidate.sortKey);
        ++result.batches.back().drawCount;
    }

    result.stats.emittedDrawCount = static_cast<u32>(result.draws.size());
    result.stats.stateBatchCount = static_cast<u32>(result.batches.size());
    return result;
}

[[nodiscard]] inline RHIPreparedIndexedDraws PrepareOpaqueIndexedDraws(
    const std::vector<RHIIndexedDrawItem>& items,
    const RHIDrawPreparationOptions& options = {}) {
    return PrepareOpaqueIndexedDraws(
        std::span<const RHIIndexedDrawItem>(items.data(), items.size()), options);
}

} // namespace rhi::renderer
