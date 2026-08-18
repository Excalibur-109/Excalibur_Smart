#include "RHI/Renderer/RHIDrawPreparation.hpp"

#include <iostream>
#include <vector>

namespace {

[[nodiscard]] RHI::RHIDrawIndexedCommand MakeDraw(
    u64 pipeline,
    u64 bindSet,
    u64 vertexBuffer,
    u64 indexBuffer,
    u32 firstInstance) {
    RHI::RHIDrawIndexedCommand draw{};
    draw.pipeline = RHI::RHIPipeline(pipeline);
    draw.bindSets = {RHI::RHIBindSet(bindSet)};
    draw.vertexStreams = {{RHI::RHIBuffer(vertexBuffer), 0, 0, 32}};
    draw.indexStream.buffer = RHI::RHIBuffer(indexBuffer);
    draw.indexStream.indexType = RHI::RHIIndexType::UInt32;
    draw.indexStream.indexCount = 36;
    draw.indexCount = 36;
    draw.instanceCount = 1;
    draw.firstInstance = firstInstance;
    return draw;
}

[[nodiscard]] rhi::renderer::RHIFrustum UnitCubeFrustum() {
    rhi::renderer::RHIFrustum frustum{};
    frustum.planes = {{
        {{1.0F, 0.0F, 0.0F}, 1.0F},
        {{-1.0F, 0.0F, 0.0F}, 1.0F},
        {{0.0F, 1.0F, 0.0F}, 1.0F},
        {{0.0F, -1.0F, 0.0F}, 1.0F},
        {{0.0F, 0.0F, 1.0F}, 1.0F},
        {{0.0F, 0.0F, -1.0F}, 1.0F},
    }};
    return frustum;
}

[[nodiscard]] bool Expect(bool value, const char* message) {
    if (!value) {
        std::cerr << "RHIDrawPreparationTests failure: " << message << '\n';
    }
    return value;
}

} // namespace

int main() {
    std::vector<rhi::renderer::RHIIndexedDrawItem> items;

    rhi::renderer::RHIIndexedDrawItem culled{};
    culled.command = MakeDraw(1, 10, 20, 30, 0);
    culled.bounds = {{-3.0F, 0.0F, 0.0F}, 0.25F};
    culled.hasBounds = true;
    items.push_back(culled);

    for (u32 instance = 0; instance < 3; ++instance) {
        rhi::renderer::RHIIndexedDrawItem item{};
        item.command = MakeDraw(1, 10, 20, 30, instance);
        item.sortKey = {0, 10, 30, 0.0F};
        item.allowInstanceMerge = true;
        items.push_back(item);
    }

    rhi::renderer::RHIIndexedDrawItem laterLayer{};
    laterLayer.command = MakeDraw(2, 40, 20, 31, 0);
    laterLayer.sortKey = {1, 40, 31, 0.0F};
    items.push_back(laterLayer);

    const rhi::renderer::RHIFrustum frustum = UnitCubeFrustum();
    rhi::renderer::RHIDrawPreparationOptions options{};
    options.frustum = &frustum;
    options.workerCount = 3;
    options.parallelThreshold = 1;
    const rhi::renderer::RHIPreparedIndexedDraws prepared =
        rhi::renderer::PrepareOpaqueIndexedDraws(items, options);

    bool success = true;
    success &= Expect(prepared.stats.inputDrawCount == 5, "input draw count");
    success &= Expect(prepared.stats.culledDrawCount == 1, "frustum culling");
    success &= Expect(prepared.stats.visibleDrawCount == 4, "visible draw count");
    success &= Expect(prepared.stats.emittedDrawCount == 2, "instance merge result");
    success &= Expect(prepared.stats.mergedDrawCount == 2, "merged draw count");
    success &= Expect(prepared.stats.stateBatchCount == 2, "state batch count");
    success &= Expect(prepared.draws[0].pipeline.value == 1, "opaque sort ordering");
    success &= Expect(prepared.draws[0].instanceCount == 3, "merged instance count");
    success &= Expect(prepared.draws[0].firstInstance == 0, "merged first instance");
    success &= Expect(prepared.draws[1].pipeline.value == 2, "render layer ordering");

    std::vector<rhi::renderer::RHIIndexedDrawItem> layeredItems(2);
    layeredItems[0].command = MakeDraw(3, 50, 60, 70, 0);
    layeredItems[0].sortKey = {0, 50, 70, 0.0F};
    layeredItems[0].allowInstanceMerge = true;
    layeredItems[1].command = MakeDraw(3, 50, 60, 70, 1);
    layeredItems[1].sortKey = {1, 50, 70, 0.0F};
    layeredItems[1].allowInstanceMerge = true;
    const rhi::renderer::RHIPreparedIndexedDraws layeredPrepared =
        rhi::renderer::PrepareOpaqueIndexedDraws(layeredItems);
    success &= Expect(layeredPrepared.draws.size() == 2, "instance merge must respect render layer");
    return success ? 0 : 1;
}
