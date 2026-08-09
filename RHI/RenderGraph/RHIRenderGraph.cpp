#include "RHIRenderGraph.hpp"

#include <algorithm>
#include <deque>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace RHI {

namespace {

constexpr u64 FNV_OFFSET = 14695981039346656037ULL;
constexpr u64 FNV_PRIME  = 1099511628211ULL;

void HashBytes(u64& hash, const void* data, std::size_t size) noexcept {
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t index = 0; index < size; ++index) {
        hash ^= bytes[index];
        hash *= FNV_PRIME;
    }
}

template <typename Type>
void HashValue(u64& hash, const Type& value) noexcept {
    HashBytes(hash, &value, sizeof(Type));
}

void HashString(u64& hash, const std::string& value) noexcept {
    HashBytes(hash, value.data(), value.size());
    const u8 terminator = 0xFF;
    HashValue(hash, terminator);
}

struct ResourceKey {
    b8 buffer = false;
    u32 index = RHI_INVALID_INDEX;

    friend b8 operator==(ResourceKey lhs, ResourceKey rhs) noexcept = default;
};

struct ResourceKeyHash {
    [[nodiscard]] std::size_t operator()(ResourceKey key) const noexcept {
        return (static_cast<std::size_t>(key.index) << 1U) | static_cast<std::size_t>(key.buffer);
    }
};

struct PassUsage {
    RHIRenderGraphResourceId resource{};
    RHIResourceState state  = RHIResourceState::Undefined;
    RHIPipelineStage stages = RHIPipelineStage::None;
    RHIAccessFlags   access = RHIAccessFlags::None;
    b8 reads = false;
    b8 writes = false;
    b8 discardContents = false;
};

struct PassBuildData {
    std::vector<PassUsage> usages;
    std::unordered_map<ResourceKey, u32, ResourceKeyHash> usageIndices;
    std::unordered_set<u32> dependencies;
    u32 workloadIndex = RHI_INVALID_INDEX;
    b8 root = false;
};

struct HazardState {
    u32 lastWriter = RHI_INVALID_INDEX;
    std::vector<u32> readers;
    b8 initialized = false;
};

struct TrackedState {
    RHIResourceState state  = RHIResourceState::Undefined;
    RHIPipelineStage stages = RHIPipelineStage::TopOfPipe;
    RHIAccessFlags access = RHIAccessFlags::None;
    RHIQueueType queue = RHIQueueType::Graphics;
    b8 initialized = false;
    b8 lastAccessWrote = false;
};

[[nodiscard]] b8 IsImported(const RHIRenderGraphBufferDesc& resource) noexcept {
    return resource.imported || RHIHasAny(resource.flags, RHIRenderGraphResourceFlags::Imported);
};

[[nodiscard]] b8 IsImported(const RHIRenderGraphTextureDesc& resource) noexcept {
    return resource.imported || RHIHasAny(resource.flags, RHIRenderGraphResourceFlags::Imported);
}

[[nodiscard]] b8 IsOutput(RHIRenderGraphResourceFlags flags) noexcept {
    return RHIHasAny(flags, RHIRenderGraphResourceFlags::Exported | RHIRenderGraphResourceFlags::NeverCull);
}

[[nodiscard]] b8 CanAlias(const RHIRenderGraphBufferDesc& resource) noexcept {
    return !IsImported(resource) &&
        RHIHasAny(resource.flags, RHIRenderGraphResourceFlags::AllowAliasing) &&
        (RHIHasAny(resource.flags, RHIRenderGraphResourceFlags::Transient) ||
        resource.desc.lifetime == RHIResourceLifetime::Transient) &&
        !IsOutput(resource.flags);
}

[[nodiscard]] b8 CanAlias(const RHIRenderGraphTextureDesc& resource) noexcept {
    return !IsImported(resource) &&
        RHIHasAny(resource.flags, RHIRenderGraphResourceFlags::AllowAliasing) &&
        (RHIHasAny(resource.flags, RHIRenderGraphResourceFlags::Transient) ||
        resource.desc.lifetime == RHIResourceLifetime::Transient) &&
        !IsOutput(resource.flags);
}

[[nodiscard]] b8 AreCompatible(const RHIBufferDesc& lhs, const RHIBufferDesc& rhs) noexcept {
    return lhs.size == rhs.size &&
           lhs.usage == rhs.usage &&
           lhs.flags == rhs.flags &&
           lhs.memoryUsage == rhs.memoryUsage &&
           lhs.lifetime == rhs.lifetime &&
           lhs.persistentlyMapped == rhs.persistentlyMapped;
}

[[nodiscard]] b8 AreCompatible(const RHITextureDesc& lhs, const RHITextureDesc& rhs) noexcept {
    return lhs.dimension == rhs.dimension &&
           lhs.extent.width == rhs.extent.width &&
           lhs.extent.height == rhs.extent.height &&
           lhs.extent.depth == rhs.extent.depth &&
           lhs.arrayLayers == rhs.arrayLayers &&
           lhs.mipLevels == rhs.mipLevels &&
           lhs.format == rhs.format &&
           lhs.samples == rhs.samples &&
           lhs.usage == rhs.usage &&
           lhs.flags == rhs.flags &&
           lhs.lifetime == rhs.lifetime &&
           lhs.initialState == rhs.initialState;
}

[[nodiscard]] RHIRenderGraphResourceType NormalizeType(RHIRenderGraphResourceType type) noexcept {
    return type == RHIRenderGraphResourceType::Buffer ? RHIRenderGraphResourceType::Buffer : RHIRenderGraphResourceType::Texture;
}

[[nodiscard]] ResourceKey MakeKey(RHIRenderGraphResourceId resource) noexcept {
    return ResourceKey(resource.IsBuffer(), resource.index);
}

void AddUnique(std::vector<u32>& values, u32 value) {
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

[[nodiscard]] std::vector<u32> BuildStableTopologicalOrder(const std::vector<PassBuildData>& passData) {
    std::vector<std::vector<u32>> dependents(passData.size());
    std::vector<u32> indegrees(passData.size(), 0);
    std::deque<u32> ready;

    for (u32 passIndex = 0; passIndex < passData.size(); ++passIndex) {
        indegrees[passIndex] = static_cast<u32>(passData[passIndex].dependencies.size());
        for (const u32 dependency : passData[passIndex].dependencies) {
            AddUnique(dependents[dependency], passIndex);
        }
        if (indegrees[passIndex] == 0) {
            ready.push_back(passIndex);
        }
    }

    std::vector<u32> order;
    order.reserve(passData.size());
    while (!ready.empty()) {
        const u32 passIndex = ready.front();
        ready.pop_front();
        order.push_back(passIndex);
        for (const u32 dependent : dependents[passIndex]) {
            if (--indegrees[passIndex] == 0) {
                const auto position = std::upper_bound(ready.begin(), ready.end(), dependent);
                ready.insert(position, dependent);
            }
        }
    }
    return order;
}

} // namesapce

std::string RHIRenderGraphCompileResult::ErrorMessage() const {
    std::ostringstream message;
    for (std::size_t index = 0; index < errors.size(); ++index) {
        if (index != 0) {
            message << '\n';
        }
        message << errors[index];
    }
    return message.str();
}

} // namespace RHI