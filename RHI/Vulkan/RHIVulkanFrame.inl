#pragma once

#include "RHIVulkanPrivate.inl"

namespace RHI {

// 本文件把已编译的 RenderGraph 与当前帧数据翻译为 Vulkan 命令，并完成一次图形队列提交。

bool RHIVulkan::RecordAndSubmitFrame(
    const RHIFramePacket& packet, // 当前帧的动态输入：上传、pass、workload、present 等。
    const RHIRenderGraphExecutionPlan& graphPlan, // 编译后的拓扑排序、资源槽位与状态转换。
    std::string* errorMessage) { // 可选错误输出；函数失败时写入可读诊断。
    // 这是 RHI 一帧在 Vulkan 后端的汇合点：CPU 上传数据、根据执行计划创建 transient
    // 资源、写入 barrier 和 draw/dispatch，最后将一个 command buffer 提交到 graphics queue。
    // graphPlan 仅描述已编译的静态依赖/资源分配；packet 保存当前帧的动态数据，例如上传字节、
     // clear value、draw 参数和导入资源的实际句柄。
    Impl::FrameContext* frame = nullptr; // 当前轮转槽位；异常路径依靠它回收未提交 staging。
    bool frameSubmitted = false; // vkQueueSubmit 成功后置位，决定资源是否需要延迟回收。

    // 这些句柄只代表本次 RenderGraph 为内部 transient 资源创建的 RHI 对象。
    // 成功提交后调用 Destroy 不会立刻销毁仍被 GPU 使用的 native 对象：Vulkan 资源层
    // 会按 submission serial 延迟回收。异常发生在提交前时则可以直接清理。
    std::vector<RHIBuffer> transientBuffers; // 仅由本 RenderGraph 执行创建的临时 buffer。
    std::vector<RHITexture> transientTextures; // 仅由本 RenderGraph 执行创建的临时 image。
    std::vector<RHITextureView> transientTextureViews; // 上述 image 创建出的临时 view。

    const auto releaseTransientResources = [&]() noexcept { // 成功和异常路径共用的释放闭包。
        // view 依赖 texture，texture/buffer 又可能仍被本帧 command buffer 引用，因此按
        // 依赖的反方向发起销毁。提交后 Destroy 会延迟 native 销毁；提交前异常则立即安全释放。
        for (auto view = transientTextureViews.rbegin(); // 从最后创建的 view 开始逆序释放。
             view != transientTextureViews.rend();
             ++view) {
            Destroy(*view); // view 先于其引用的 VkImage 释放。
        }
        for (auto texture = transientTextures.rbegin(); // view 释放后再逆序处理 texture。
             texture != transientTextures.rend();
             ++texture) {
            Destroy(*texture); // 已提交时内部转入按 timeline serial 的延迟销毁队列。
        }
        for (auto buffer = transientBuffers.rbegin(); // 最后释放临时 buffer。
             buffer != transientBuffers.rend();
             ++buffer) {
            Destroy(*buffer); // 保证成功提交后 VkBuffer 不会在 GPU 使用期间被销毁。
        }
        transientTextureViews.clear(); // 防止 catch 路径二次释放同一 view 句柄。
        transientTextures.clear(); // 清空 texture 所有权清单。
        transientBuffers.clear(); // 清空 buffer 所有权清单。
    };

    try {
        if (!IsInitialized() || impl_->frameContexts.empty()) {
            throw std::runtime_error("RHIVulkan is not initialized or has no frame contexts");
        }

        frame = &impl_->prepareNextFrameContext(); // 等待槽位、回收其 staging，并重置命令缓冲。
        // prepareNextFrameContext 会等待这个轮转槽位对应的 timeline 值，释放上一轮的
        // staging 资源并重置 command buffer。因此 CPU 最多领先 GPU framesInFlight 帧。
        VkCommandBuffer commandBuffer = frame->commandBuffer; // 本帧所有 vkCmd* 写入的目标。

        VkCommandBufferBeginInfo beginInfo{}; // 命令录制的配置；{} 将所有可选字段清零。
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO; // Vulkan 结构类型标签。
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // 录制结果只提交一次，下一轮重置后复用。
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) { // 进入录制状态；失败通常表示 command buffer 未正确 reset 或设备异常。
            throw std::runtime_error("vkBeginCommandBuffer failed");
        }

        // 上传命令本身是 TransferWrite；copy 之后必须把这次写入对资源最终用途可见。
        // 下面两个函数把 RHI 的 usage 转为这个“第一次消费者”所需的 access/stage。
        const auto bufferDstAccess = [](RHIBufferUsage usage) { // 推导 copy 写入之后的目标访问掩码。
            VkAccessFlags access = 0; // 累积 VkAccessFlagBits；0 表示 usage 尚未映射到专用访问。
            if (RHIHasAny(usage, RHIBufferUsage::Vertex))               access |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT; // 顶点提取器读取 attribute。
            if (RHIHasAny(usage, RHIBufferUsage::Index))                access |= VK_ACCESS_INDEX_READ_BIT; // 索引提取器读取 index。
            if (RHIHasAny(usage, RHIBufferUsage::Uniform))              access |= VK_ACCESS_UNIFORM_READ_BIT; // shader 从 uniform buffer 读取。
            if (RHIHasAny(usage, RHIBufferUsage::Storage))              access |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT; // storage buffer 可读写。
            if (RHIHasAny(usage, RHIBufferUsage::Indirect))             access |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT; // indirect 参数由命令处理器读取。
            if (RHIHasAny(usage, RHIBufferUsage::TransferSource))       access |= VK_ACCESS_TRANSFER_READ_BIT; // 后续 copy 作为源读取。
            if (RHIHasAny(usage, RHIBufferUsage::TransferDestination))  access |= VK_ACCESS_TRANSFER_WRITE_BIT; // 后续 copy 作为目的写入。
            return access == 0 ? VK_ACCESS_MEMORY_READ_BIT : access; // 未知 usage 使用保守的通用内存读访问。
        };

        const auto bufferDstStages = [](RHIBufferUsage usage) { // 推导执行上述目标访问的流水线阶段。
            VkPipelineStageFlags stages = 0; // 累积 VkPipelineStageFlagBits。
            if (RHIHasAny(usage, RHIBufferUsage::Vertex) ||
                RHIHasAny(usage, RHIBufferUsage::Index)) {
                stages |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT; // Vertex/Index 均由 vertex-input 阶段消费。
            }
            if (RHIHasAny(usage, RHIBufferUsage::Uniform) ||
                RHIHasAny(usage, RHIBufferUsage::Storage)) {
                stages |= VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT | // 图形 shader 的任意阶段都可能访问。
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT; // compute shader 同样可能访问。
            }
            if (RHIHasAny(usage, RHIBufferUsage::Indirect)) {
                stages |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT; // GPU 在执行 indirect draw 前读取参数。
            }
            if (RHIHasAny(usage, RHIBufferUsage::TransferSource | RHIBufferUsage::TransferDestination)) {
                stages |= VK_PIPELINE_STAGE_TRANSFER_BIT; // copy/fill/resolve 属于 transfer 阶段。
            }
            return stages == 0 ? VK_PIPELINE_STAGE_ALL_COMMANDS_BIT : stages; // 没有专用映射时保守覆盖所有命令。
        };

        std::vector<VkBufferMemoryBarrier> uploadBarriers; // 聚合所有 buffer upload 的内存依赖。
        std::vector<RHIBuffer> uploadedBuffers; // 与 barrier 同步更新 currentState 的 RHI 句柄。
        VkPipelineStageFlags uploadDestinationStages = 0; // 所有上传目标阶段的并集。
        for (const RHIBufferUploadDesc& upload : packet.uploads.buffers) {
            // 每个 CPU blob 都经过独立的 host-visible staging buffer。目标 buffer 可为
            // device local；CPU 永远不会直接 map 目标 buffer。
            if (upload.data.empty()) {
                continue;
            }

            Impl::BufferResource* destination = getRenderResource(impl_->buffers, upload.destination); // 从不透明 RHI 句柄取出 VkBuffer 资源。
            if (destination == nullptr || destination->buffer == VK_NULL_HANDLE) {
                throw std::runtime_error("RHIFramePacket uploads contain an invalid destination buffer");
            }
            if (upload.destinationOffset > destination->desc.size ||
                upload.data.size() >
                    destination->desc.size - upload.destinationOffset) {
                throw std::runtime_error(
                    "RHIFramePacket buffer upload range exceeds destination buffer size");
            }

            Impl::StagingResource staging{}; // 局部 staging 所有权，压入 FrameContext 后由其持续持有。
            VkBufferCreateInfo stagingInfo{}; // 创建 host-visible 的 VkBuffer 所需参数。
            stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO; // Vulkan 结构类型标签。
            stagingInfo.size = static_cast<VkDeviceSize>(upload.data.size()); // staging buffer 精确容纳本 upload 字节数。
            stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT; // 它只作为 vkCmdCopyBuffer 的源。
            stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // 仅 graphics queue family 使用，不需跨族并发访问。
            if (vkCreateBuffer(impl_->native.device, &stagingInfo, nullptr, &staging.buffer) != VK_SUCCESS) {
                throw std::runtime_error("vkCreateBuffer(staging) failed");
            }

            // buffer 创建成功后立即登记所有权，后续 memory allocation 失败时也能由 cleanup 回收。
            frame->stagingResources.push_back(staging); // GPU 完成前把 staging 所有权固定在 FrameContext。
            Impl::StagingResource& trackedStaging = frame->stagingResources.back(); // 引用 vector 中真正受清理逻辑管理的元素。

            VkMemoryRequirements requirements{}; // Vulkan 给出的大小、对齐与可用 memory type 位集。
            vkGetBufferMemoryRequirements(impl_->native.device, trackedStaging.buffer, &requirements); // 查询该 VkBuffer 的分配要求。
            VkMemoryAllocateInfo memoryInfo{}; // 分配 VkDeviceMemory 的参数。
            memoryInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO; // Vulkan 结构类型标签。
            memoryInfo.allocationSize = requirements.size; // 必须按驱动要求的大小，而不只是 upload 字节数分配。
            memoryInfo.memoryTypeIndex = impl_->findMemoryType( // 选择同时支持 CPU map 与 coherent 写入的 heap 类型。
                requirements.memoryTypeBits,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            if (vkAllocateMemory(impl_->native.device, &memoryInfo, nullptr, &trackedStaging.memory) != VK_SUCCESS) { // 分配 staging 的 VkDeviceMemory。
                throw std::runtime_error("vkAllocateMemory(staging) failed");
            }
            // 后续 bind/map 都操作 trackedStaging，保证 cleanup 看到最新 buffer/memory 句柄。
            if (vkBindBufferMemory(impl_->native.device, trackedStaging.buffer, trackedStaging.memory, 0) != VK_SUCCESS) { // 以 offset 0 将分配绑定到 VkBuffer。
                throw std::runtime_error("vkBindBufferMemory(staging) failed");
            }

            void* mapped = nullptr; // vkMapMemory 返回的 CPU 可写地址。
            if (vkMapMemory(impl_->native.device, trackedStaging.memory, 0, stagingInfo.size, 0, &mapped) != VK_SUCCESS) { // 映射完整 staging 分配供 CPU memcpy。
                throw std::runtime_error("vkMapMemory(staging) failed");
            }
            std::memcpy(mapped, upload.data.data(), upload.data.size()); // 复制 packet 持有的 CPU 字节到 coherent 映射内存。
            vkUnmapMemory(impl_->native.device, trackedStaging.memory); // 解除 CPU 映射；HOST_COHERENT 因而无需 vkFlushMappedMemoryRanges。

            VkBufferCopy copy{}; // 一个 staging-buffer 到 destination-buffer 的 copy 区域。
            copy.srcOffset = 0; // staging 中的数据从第一个字节开始。
            copy.dstOffset = static_cast<VkDeviceSize>(upload.destinationOffset); // RHI 描述指定的目标字节偏移。
            copy.size = static_cast<VkDeviceSize>(upload.data.size()); // copy 的有效字节数。
            vkCmdCopyBuffer(commandBuffer, trackedStaging.buffer, destination->buffer, 1, &copy); // 录制 copy；实际执行发生在 queue submit 后。

            // 这不是 layout transition，而是纯内存可见性 barrier：保证 vkCmdCopyBuffer
            // 写入 destination 后，后续 VertexInput/Shader/Indirect 等读取阶段能看见它。
            VkBufferMemoryBarrier barrier{}; // 使 transfer 写入对后续 consumer 可见的 buffer barrier。
            barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER; // Vulkan 结构类型标签。
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; // 生产者是 vkCmdCopyBuffer 的 transfer write。
            barrier.dstAccessMask = bufferDstAccess(destination->desc.usage); // 消费者访问由 buffer usage 推导。
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // 本实现不在 queue family 之间转移所有权。
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // 与源字段成对保持 IGNORED。
            barrier.buffer = destination->buffer; // 要同步的 native VkBuffer。
            barrier.offset = upload.destinationOffset; // 只同步本 upload 覆盖的起始字节。
            barrier.size = upload.data.size(); // 只同步本 upload 覆盖的长度。
            uploadBarriers.push_back(barrier); // 延后合并为一次 vkCmdPipelineBarrier，减少命令数。
            uploadedBuffers.push_back(upload.destination); // 保存句柄，稍后更新内部状态缓存。
            uploadDestinationStages |= bufferDstStages(destination->desc.usage); // 合并所有消费者阶段。
        }

        if (!uploadBarriers.empty()) {
            vkCmdPipelineBarrier(
                commandBuffer, // 接收这条同步命令的当前 VkCommandBuffer。
                VK_PIPELINE_STAGE_TRANSFER_BIT, // sourceStageMask：copy 写入发生于 transfer 阶段。
                uploadDestinationStages, // destinationStageMask：所有后续 buffer 消费阶段的并集。
                0, // dependencyFlags：不使用 by-region 或 device-group 依赖。
                0, // memoryBarrierCount：没有覆盖全部内存的 VkMemoryBarrier。
                nullptr, // pMemoryBarriers：对应 count 为 0，必须为空。
                static_cast<u32>(uploadBarriers.size()), // buffer barrier 数量。
                uploadBarriers.data(), // 指向连续 VkBufferMemoryBarrier 数组的第一个元素。
                0, // imageMemoryBarrierCount：本批只同步 buffer，不同步 image。
                nullptr); // pImageMemoryBarriers：对应 count 为 0，必须为空。

            // 上面的 barrier 已经让 transfer 写入对 buffer 的声明用途可见。同步更新
            // 追踪状态，后续 RenderGraph transition 才会使用真实的 source stage/access，
            // 而不是误以为资源仍停留在 TopOfPipe/None。
            for (const RHIBuffer handle : uploadedBuffers) { // 逐个更新已上传资源的状态缓存。
                Impl::BufferResource* buffer = getRenderResource(impl_->buffers, handle);
                if (buffer != nullptr) {
                    buffer->currentState = RHIResourceState::Common; // upload 后不强加语义布局，保留为通用 RHI 状态。
                    buffer->currentStages = uploadDestinationStages; // 记录 barrier 的 destination stage，供下一次 transition 作为 source。
                    buffer->currentAccess = bufferDstAccess(buffer->desc.usage); // 记录消费者访问掩码。
                }
            }
        }

        // graphBuffers 是“逻辑资源下标 -> RHI 句柄”；physicalGraphBuffers 是“物理槽
        // -> RHI 句柄”。Imported 逻辑资源直接引用 packet 的外部句柄，内部资源才按
        // allocation slot 创建。多个生命周期不重叠的逻辑资源会得到同一个物理句柄。
        std::vector<RHIBuffer> graphBuffers(packet.graph.buffers.size()); // logical buffer index -> 实际 RHI handle。
        std::vector<RHIBuffer> physicalGraphBuffers(graphPlan.bufferAllocationCount); // physical allocation slot -> 实际 RHI handle。
        for (u32 index = 0; index < packet.graph.buffers.size(); ++index) {
            // 被 RenderGraph cull 的资源 lifetime 没有 firstPass；既不创建，也不会出现在
            // compiled pass transition 中。
            if (graphPlan.bufferLifetimes[index].firstPass == RHI_INVALID_INDEX) {
                continue;
            }
            const RHIRenderGraphBufferDesc& graphBuffer = packet.graph.buffers[index]; // 读取当前 logical buffer 的声明。
            if (graphBuffer.imported ||
                RHIHasAny(graphBuffer.flags, RHIRenderGraphResourceFlags::Imported)) {
                graphBuffers[index] = graphBuffer.externalHandle; // imported buffer 由调用方创建，直接采用外部句柄。
            } else {
                const u32 slot = graphPlan.bufferAllocationSlots[index]; // 编译器分配的可复用物理槽位。
                if (!physicalGraphBuffers[slot]) {
                    physicalGraphBuffers[slot] = CreateBuffer(graphBuffer.desc); // 首次使用该槽时创建 native VkBuffer。
                    transientBuffers.push_back(physicalGraphBuffers[slot]); // 记录所有权，帧结束后释放。
                }
                graphBuffers[index] = physicalGraphBuffers[slot]; // 生命周期重叠的 logical index 映射到自己的 physical slot。
            }
        }

        // Texture 使用相同的两级映射。view 跟随物理 texture 创建一次；逻辑资源切换
        // 发生在同一物理槽上时，由后面的 aliasing barrier 处理可见性与 layout。
        std::vector<RHITexture> graphTextures(packet.graph.textures.size()); // logical texture index -> RHI texture handle。
        std::vector<RHITexture> physicalGraphTextures(graphPlan.textureAllocationCount); // physical texture slot -> RHI texture handle。
        for (u32 index = 0; index < packet.graph.textures.size(); ++index) {
            if (graphPlan.textureLifetimes[index].firstPass == RHI_INVALID_INDEX) {
                continue;
            }
            const RHIRenderGraphTextureDesc& graphTexture = packet.graph.textures[index]; // 当前 logical texture 的静态描述。
            if (graphTexture.imported ||
                RHIHasAny(graphTexture.flags, RHIRenderGraphResourceFlags::Imported)) {
                graphTextures[index] = graphTexture.externalHandle; // imported texture 使用调用方提供的已有 image。
                continue;
            }

            const u32 slot = graphPlan.textureAllocationSlots[index]; // 生命周期分析生成的物理 image 槽。
            if (physicalGraphTextures[slot]) {
                graphTextures[index] = physicalGraphTextures[slot]; // 复用已经为该物理槽创建的 texture。
                continue;
            }

            physicalGraphTextures[slot] = CreateTexture(graphTexture.desc); // 首次使用该槽时创建 VkImage 和绑定内存。
            graphTextures[index] = physicalGraphTextures[slot]; // 建立 logical 到 physical 的映射。
            transientTextures.push_back(physicalGraphTextures[slot]); // 将其归入帧级临时资源清单。

            RHITextureViewDesc viewDesc{}; // 从整张 transient texture 构造一个供 attachment 使用的全范围 view。
            viewDesc.debugName = graphTexture.name + ".RenderGraphView"; // 便于 RenderDoc/validation 识别的调试名。
            viewDesc.texture = graphTextures[index]; // 此 view 引用的 texture 句柄。
            viewDesc.format = graphTexture.desc.format; // 不进行格式重解释，保持 texture 原格式。
            viewDesc.mipLevelCount = graphTexture.desc.mipLevels; // 覆盖全部 mip level。
            viewDesc.arrayLayerCount = graphTexture.desc.arrayLayers; // 覆盖全部 array/cube layer。
            if (graphTexture.desc.dimension == RHITextureDimension::Texture1D) {
                viewDesc.dimension = graphTexture.desc.arrayLayers > 1 // 多层 1D texture 采用数组 view。
                                         ? RHITextureViewDimension::View1DArray
                                         : RHITextureViewDimension::View1D;
            } else if (graphTexture.desc.dimension == RHITextureDimension::Texture3D) {
                viewDesc.dimension = RHITextureViewDimension::View3D; // 3D image 必须作为 3D view 访问。
            } else if (RHIHasAny(
                           graphTexture.desc.flags,
                           RHITextureCreateFlags::CubeCompatible)) {
                viewDesc.dimension = graphTexture.desc.arrayLayers > 6 // 超过一个 cube 的 6 层时采用 cube-array view。
                                         ? RHITextureViewDimension::CubeArray
                                         : RHITextureViewDimension::Cube;
            } else {
                viewDesc.dimension = graphTexture.desc.arrayLayers > 1 // 多层普通 texture 采用 2D-array view。
                                         ? RHITextureViewDimension::View2DArray
                                         : RHITextureViewDimension::View2D;
            }
            if (isDepthFormat(graphTexture.desc.format)) {
                viewDesc.aspect = RHITextureAspect::Depth; // depth 格式 view 只选择 depth aspect。
                if (hasStencilFormat(graphTexture.desc.format)) {
                    viewDesc.aspect |= RHITextureAspect::Stencil; // D24S8/D32S8 等格式还要暴露 stencil aspect。
                }
            }
            transientTextureViews.push_back(CreateTextureView(viewDesc)); // 创建 VkImageView，并登记为帧结束时需释放的资源。
        }

        const auto findViewForTexture = [&](RHITexture texture, RHITextureAspect aspect) -> RHITextureView { // 按 texture/aspect 查找已创建的 view。
            // Imported texture 的 view 由调用方/Swapchain 创建，transient texture 的 view 则
            // 刚在上方创建。附件编译计划只存 texture index，因此在录制时补回适合 aspect 的 view。
            for (u64 index = 0; index < impl_->textureViews.size(); ++index) { // RHI 句柄以槽位下标加 1 编码。
                const Impl::TextureViewResource& view = impl_->textureViews[static_cast<size_t>(index)]; // 取出此槽位的 Vulkan view 及其描述。
                if (view.view != VK_NULL_HANDLE && view.desc.texture == texture &&
                    (aspect == RHITextureAspect::All ||
                     view.desc.aspect == RHITextureAspect::All ||
                     RHIHasAny(view.desc.aspect, aspect))) {
                    return RHITextureView(index + 1); // 0 是空句柄，故槽位 N 对应公开句柄 N + 1。
                }
            }
            return {}; // 未找到有效 view 时返回空句柄，由调用方产生带上下文的错误。
        };

        const auto transitionTexture = [&](
            RHITexture handle, // 要转换的 RHI image。
            RHIResourceState after, // transition 后 RHI 语义状态，决定目标 VkImageLayout。
            RHIPipelineStage requestedStages = RHIPipelineStage::AllCommands, // 可选的精确 consumer stage；默认按 state 推导。
            RHIAccessFlags requestedAccess = RHIAccessFlags::None, // 可选的精确 consumer access；默认按 state 推导。
            bool forceBarrier = false, // true 时即使内部缓存看似相同也录制 barrier。
            bool discardContents = false, // true 表示旧像素不需要保存，可从 UNDEFINED 开始。
            bool aliasingBarrier = false) { // true 表示物理 image 被不同 logical resource 复用，不能丢失旧同步。
            // RenderGraph 只描述 pass 读写需要的 RHIResourceState；Vulkan 同步还需要回答：
            //   stage：生产/消费发生在哪一段流水线；
            //   access：该阶段读写哪类内存；
            //   layout：image 以何种专用布局被访问。
            // 三者必须匹配，单独修改 layout 并不能保证前一次写入对后一次读取可见。
            Impl::TextureResource* texture = getRenderResource(impl_->textures, handle); // 解引用 RHI 句柄，取得 VkImage 和当前状态缓存。
            if (texture == nullptr || texture->image == VK_NULL_HANDLE) {
                return;
            }

            const VkPipelineStageFlags destinationStages = // 选择下一次使用的 Vulkan consumer stage mask。
                requestedStages == RHIPipelineStage::AllCommands
                    ? stageFromResourceState(after)
                    : toVkPipelineStages(requestedStages);
            const VkAccessFlags destinationAccess = // 选择下一次使用需要的 Vulkan access mask。
                requestedAccess == RHIAccessFlags::None
                    ? accessFromResourceState(after)
                    : toVkAccessFlags(requestedAccess);
            if (!forceBarrier && texture->currentState == after &&
                texture->currentStages == destinationStages &&
                texture->currentAccess == destinationAccess) {
                return;
            }

            VkImageMemoryBarrier barrier{}; // 统一承载 image memory dependency 与 layout transition。
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER; // Vulkan 结构类型标签。
            barrier.srcAccessMask = discardContents && !aliasingBarrier // 丢弃且非 alias 时没有旧内容需要成为 source access。
                                        ? 0
                                        : texture->currentAccess;
            barrier.dstAccessMask = destinationAccess; // destination 阶段能以该 access 类型看见 source 写入。
            // 普通首次使用可从 UNDEFINED 开始；物理槽 alias 时仍需保留上一逻辑
            // 资源的真实 layout 作为同步起点，确保旧写入完成后再复用同一 VkImage。
            barrier.oldLayout = discardContents && !aliasingBarrier // 普通 discard 可用 UNDEFINED，alias 仍需保留真实旧 layout。
                                    ? VK_IMAGE_LAYOUT_UNDEFINED
                                    : toVkImageLayout(texture->currentState);
            barrier.newLayout = toVkImageLayout(after); // destination state 对应的 Vulkan 专用 image layout。
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // 无 queue-family ownership transfer。
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // 同上，source/destination 族均忽略。
            barrier.image = texture->image; // 被同步与转换 layout 的 VkImage。
            barrier.subresourceRange.aspectMask = toVkImageAspect(RHITextureAspect::All, texture->desc.format); // 覆盖格式实际存在的 color/depth/stencil aspect。
            barrier.subresourceRange.baseMipLevel = 0; // 从最粗糙 mip 开始。
            barrier.subresourceRange.levelCount = texture->desc.mipLevels; // 转换整张 image 的全部 mip。
            barrier.subresourceRange.baseArrayLayer = 0; // 从第一个 array layer 开始。
            barrier.subresourceRange.layerCount = texture->desc.arrayLayers; // 转换全部 layer；3D texture 此处对应单一 image layer。

            // discardContents 说明新的逻辑资源不需要旧像素；普通首次使用可直接声明
            // UNDEFINED，跳过保存旧内容。aliasingBarrier 则复用同一物理 VkImage，仍要以
            // 上一逻辑资源的真实 stage/access/layout 为 source，不能把同步关系一并丢掉。
            vkCmdPipelineBarrier(
                commandBuffer, // 当前帧的录制目标。
                discardContents && !aliasingBarrier
                    ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
                    : texture->currentStages,
                destinationStages, // dstStageMask：下一次 image 使用所在的阶段。
                0, // dependencyFlags：不请求额外依赖语义。
                0, // memoryBarrierCount：没有全局内存 barrier。
                nullptr, // pMemoryBarriers：count 为 0。
                0, // bufferMemoryBarrierCount：没有 buffer barrier。
                nullptr, // pBufferMemoryBarriers：count 为 0。
                1, // imageMemoryBarrierCount：本次只转换这一个 image。
                &barrier); // pImageMemoryBarriers：指向上方完整填写的 barrier。
            texture->currentState = after; // 缓存 RHI 状态，下一 transition 以它为 source。
            texture->currentStages = destinationStages; // 缓存 source stage。
            texture->currentAccess = destinationAccess; // 缓存 source access。
        };

        const auto transitionBuffer = [&] (
            RHIBuffer handle, // 要转换的 RHI buffer。
            const RHIRenderGraphTransition& transition) { // RenderGraph 给出的目标状态、stage/access 与 alias/discard 信息。
            Impl::BufferResource* buffer = getRenderResource(impl_->buffers, handle); // 取到 native VkBuffer 资源。
            if (buffer == nullptr || buffer->buffer == VK_NULL_HANDLE) {
                throw std::runtime_error("RenderGraph buffer transition has an invalid handle");
            }

            const VkPipelineStageFlags destinationStages = // 目标 consumer 执行阶段。
                transition.destinationStages == RHIPipelineStage::AllCommands
                    ? stageFromResourceState(transition.after)
                    : toVkPipelineStages(transition.destinationStages);
            const VkAccessFlags destinationAccess = // 目标 consumer 对 buffer 的访问类型。
                transition.destinationAccess == RHIAccessFlags::None
                    ? accessFromResourceState(transition.after)
                    : toVkAccessFlags(transition.destinationAccess);

            // buffer 没有 image layout，但依然需要通过 stage/access 建立前一使用和后一使用
            // 的 execution/memory dependency。这里对整个 buffer 生效，RHI 暂未细分 subrange。
            VkBufferMemoryBarrier barrier{}; // buffer 没有 layout，只需执行和内存可见性依赖。
            barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER; // Vulkan 结构类型标签。
            barrier.srcAccessMask = transition.discardContents && // 对 buffer 同样仅在非 alias discard 时忽略旧 source access。
                                            !transition.aliasingBarrier
                                        ? 0
                                        : buffer->currentAccess;
            barrier.dstAccessMask = destinationAccess; // 目标阶段需要可见的访问类型。
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // 不进行 queue family 所有权转移。
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // 同上。
            barrier.buffer = buffer->buffer; // 要同步的 native buffer。
            barrier.offset = 0; // 当前 RHI transition 没有子区间粒度，从开头覆盖。
            barrier.size = VK_WHOLE_SIZE; // 覆盖直到 VkBuffer 结尾。
            vkCmdPipelineBarrier(
                commandBuffer, // 当前 command buffer。
                transition.discardContents && !transition.aliasingBarrier
                    ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
                    : buffer->currentStages,
                destinationStages, // dstStageMask。
                0, // dependencyFlags。
                0, // memoryBarrierCount。
                nullptr, // pMemoryBarriers。
                1, // bufferMemoryBarrierCount。
                &barrier, // pBufferMemoryBarriers。
                0, // imageMemoryBarrierCount。
                nullptr); // pImageMemoryBarriers。
            buffer->currentState = transition.after; // 更新 RHI 状态缓存。
            buffer->currentStages = destinationStages; // 更新下一 transition 的 source stage。
            buffer->currentAccess = destinationAccess; // 更新下一 transition 的 source access。
        };

        for (const RHITextureUploadDesc& upload : packet.uploads.textures) { // 分别录制每个 CPU 到 image 的 upload。
            // 图片上传与 buffer 上传相同：CPU -> staging buffer -> vkCmdCopyBufferToImage。
            // 区别在于 image 在 copy 前必须先进入 TRANSFER_DST_OPTIMAL layout。
            if (upload.data.empty()) {
                continue;
            }

            Impl::TextureResource* texture =
                getRenderResource(impl_->textures, upload.destination); // 取得 upload 的目标 VkImage 及其 RHI 描述。
            if (texture == nullptr || texture->image == VK_NULL_HANDLE) {
                throw std::runtime_error(
                    "RHIFramePacket texture upload destination is invalid");
            }
            if (!RHIHasAny(
                    texture->desc.usage,
                    RHITextureUsage::TransferDestination)) {
                throw std::runtime_error(
                    "Vulkan texture upload destination is missing TransferDestination usage");
            }
            if (upload.mipLevel >= texture->desc.mipLevels ||
                upload.arrayLayer >= texture->desc.arrayLayers ||
                upload.extent.width == 0 || upload.extent.height == 0 ||
                upload.extent.depth == 0 || upload.offset.x < 0 ||
                upload.offset.y < 0 || upload.offset.z < 0) {
                throw std::runtime_error("Vulkan texture upload subresource is invalid");
            }
            if (upload.bytesPerRow != 0 || upload.rowsPerImage != 0) {
                throw std::runtime_error(
                    "Vulkan texture uploads currently require tightly packed source data");
            }

            const u32 mipWidth = std::max( // 本次目标 mip 的实际宽度，mip 不会缩小到 0。
                1u, texture->desc.extent.width >> upload.mipLevel);
            const u32 mipHeight = std::max( // 本次目标 mip 的实际高度。
                1u, texture->desc.extent.height >> upload.mipLevel);
            const u32 mipDepth = texture->desc.dimension == // 2D/1D array 的 image depth 固定为 1；只有 3D mip 缩深度。
                                         RHITextureDimension::Texture3D
                                     ? std::max(
                                           1u,
                                           texture->desc.extent.depth >>
                                               upload.mipLevel)
                                     : 1u;
            if (static_cast<u32>(upload.offset.x) > mipWidth ||
                upload.extent.width >
                    mipWidth - static_cast<u32>(upload.offset.x) ||
                static_cast<u32>(upload.offset.y) > mipHeight ||
                upload.extent.height >
                    mipHeight - static_cast<u32>(upload.offset.y) ||
                static_cast<u32>(upload.offset.z) > mipDepth ||
                upload.extent.depth >
                    mipDepth - static_cast<u32>(upload.offset.z)) {
                throw std::runtime_error("Vulkan texture upload range is invalid");
            }

            transitionTexture(
                upload.destination, // 目标 texture 句柄。
                RHIResourceState::CopyDestination, // 令 toVkImageLayout 选出 TRANSFER_DST_OPTIMAL。
                RHIPipelineStage::Transfer, // copy 命令所在 Vulkan transfer 阶段。
                RHIAccessFlags::TransferWrite); // copy 将写入目标 image。

            // staging resource 挂到 FrameContext，而不是函数局部直接销毁；GPU 在 submit 后
            // 仍会读取它，只有该帧 completionValue 完成后才能释放。
            Impl::StagingResource staging{}; // 本 texture upload 专用的 staging buffer/memory 对。
            VkBufferCreateInfo stagingInfo{}; // 创建 staging VkBuffer 的参数。
            stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO; // Vulkan 结构类型标签。
            stagingInfo.size = static_cast<VkDeviceSize>(upload.data.size()); // source 数据的紧密字节总数。
            stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT; // 它将作为 vkCmdCopyBufferToImage 的 source。
            stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // 只由 graphics queue family 消费。
            if (vkCreateBuffer(
                    impl_->native.device,
                    &stagingInfo,
                    nullptr,
                    &staging.buffer) != VK_SUCCESS) {
                throw std::runtime_error(
                    "vkCreateBuffer(texture staging) failed");
            }
            frame->stagingResources.push_back(staging); // 移交给 FrameContext，直到此帧 GPU 完成才能释放。
            Impl::StagingResource& trackedStaging = frame->stagingResources.back();

            VkMemoryRequirements requirements{}; // 驱动给出的 buffer 内存分配约束。
            vkGetBufferMemoryRequirements(
                impl_->native.device,
                trackedStaging.buffer,
                &requirements);
            VkMemoryAllocateInfo memoryInfo{}; // staging VkDeviceMemory 的分配参数。
            memoryInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO; // Vulkan 结构类型标签。
            memoryInfo.allocationSize = requirements.size; // 遵从驱动最小分配大小和对齐。
            memoryInfo.memoryTypeIndex = impl_->findMemoryType( // texture staging 同样选择 host-visible + coherent memory type。
                requirements.memoryTypeBits,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            if (vkAllocateMemory(
                    impl_->native.device,
                    &memoryInfo,
                    nullptr,
                    &trackedStaging.memory) != VK_SUCCESS) {
                throw std::runtime_error(
                    "vkAllocateMemory(texture staging) failed");
            }
            if (vkBindBufferMemory( // 将 allocation 绑定到 texture staging buffer；offset 为 0。
                    impl_->native.device,
                    trackedStaging.buffer,
                    trackedStaging.memory,
                    0) != VK_SUCCESS) {
                throw std::runtime_error(
                    "vkBindBufferMemory(texture staging) failed");
            }

            void* mapped = nullptr; // CPU 映射地址。
            if (vkMapMemory( // 映射完整 texture staging buffer，供紧密数据 memcpy。
                    impl_->native.device,
                    trackedStaging.memory,
                    0,
                    stagingInfo.size,
                    0,
                    &mapped) != VK_SUCCESS) {
                throw std::runtime_error("vkMapMemory(texture staging) failed");
            }
            std::memcpy(mapped, upload.data.data(), upload.data.size()); // 将紧密排列的 texel 字节复制到 staging。
            vkUnmapMemory(impl_->native.device, trackedStaging.memory); // HOST_COHERENT 内存无需显式 flush。

            VkBufferImageCopy copy{}; // 定义 staging buffer 字节如何映射到 image 子资源区域。
            copy.imageSubresource.aspectMask = toVkImageAspect( // color/depth/stencil 格式对应的 image aspect。
                RHITextureAspect::All,
                texture->desc.format);
            copy.imageSubresource.mipLevel = upload.mipLevel; // 只写入 upload 指定的 mip。
            copy.imageSubresource.baseArrayLayer = upload.arrayLayer; // 只写入 upload 指定的首层。
            copy.imageSubresource.layerCount = 1; // 单条 RHI upload 每次只覆盖一个 array layer。
            copy.imageOffset = { upload.offset.x, upload.offset.y, upload.offset.z}; // 目标 mip 内的 texel 起始坐标。
            copy.imageExtent = { // 要写入的 width/height/depth texel 尺寸。
                upload.extent.width,
                upload.extent.height,
                upload.extent.depth};
            vkCmdCopyBufferToImage(
                commandBuffer, // 当前 command buffer。
                trackedStaging.buffer, // 已填入 CPU 数据的 transfer-source buffer。
                texture->image, // 被写入的 VkImage。
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // 必须与上方 transitionTexture 的目标 layout 相同。
                1, // regionCount：只提供一个 VkBufferImageCopy。
                &copy); // pRegions：指向上述 image 子资源区域。
        }

        const auto vkClearColor = [](const RHIClearColor& color) { // 将 RHI 浮点清屏色变为 Vulkan union。
            VkClearValue value{}; // 清零后只填 color.float32 成员。
            value.color.float32[0] = color.r; // 红色通道。
            value.color.float32[1] = color.g; // 绿色通道。
            value.color.float32[2] = color.b; // 蓝色通道。
            value.color.float32[3] = color.a; // alpha 通道。
            return value; // 值返回，供 VkRenderingAttachmentInfo::clearValue 使用。
        };

        const auto vkClearDepthStencil = [](const RHIClearDepthStencil& clear) { // 转换深度/模板 attachment 的清屏值。
            VkClearValue value{}; // 清零后选择 depthStencil union 成员。
            value.depthStencil.depth = clear.depth; // 浮点深度清屏值，通常是反/正 Z 约定的远端值。
            value.depthStencil.stencil = clear.stencil; // 整数模板清屏值。
            return value; // 返回给 dynamic rendering depth attachment。
        };

        // plan.passes 已经完成拓扑排序和裁剪，因此后端只需线性录制。每个 pass 先执行
        // 编译器生成的 transition，再录制 workload；这条顺序就是依赖真正落到 GPU
        // command stream 的位置。
        for (const RHICompiledRenderGraphPass& compiledPass : graphPlan.passes) { // 已拓扑排序的 pass 按最终 GPU 执行顺序录制。
            // sourcePass 保存构建本帧 attachment 的动态数据；compiledPass 保存经过排序、
            // cull、资源分配后稳定不变的索引和 transition 列表。
            const RHIRenderGraphPassDesc& sourcePass = packet.graph.passes[compiledPass.sourcePassIndex]; // 通过稳定 source index 找回本帧动态 attachment 参数。
            for (const RHIRenderGraphTransition& transition : compiledPass.transitions) {
                if (transition.resource.IsBuffer()) {
                    transitionBuffer(
                        graphBuffers[transition.resource.index],
                        transition);
                } else {
                    transitionTexture(
                        graphTextures[transition.resource.index],
                        transition.after,
                        transition.destinationStages,
                        transition.destinationAccess,
                        true,
                        transition.discardContents,
                        transition.aliasingBarrier);
                }
            }

            const RHIRenderPassWorkload* workload = // pass 没有命令 workload 时置空，但仍可能仅用于 attachment clear。
                compiledPass.workloadIndex == RHI_INVALID_INDEX
                    ? nullptr
                    : &packet.workloads[compiledPass.workloadIndex];
            const bool hasAttachments = !compiledPass.colorAttachments.empty() || // 是否有至少一个 color target。
                                        compiledPass.depthStencilAttachment.has_value(); // 或 depth/stencil target。
            if (workload == nullptr && !hasAttachments) {
                continue;
            }

            if (workload != nullptr) {
                // barrier 的唯一事实来源是 RenderGraph reads/writes。若同时接受 workload
                // 手写 barrier，就会出现两套状态追踪互相覆盖。尚未实现的命令也必须
                // 显式失败，不能静默跳过后得到“成功提交但画面错误”的结果。
                if (!workload->barriers.globals.empty() ||
                    !workload->barriers.textures.empty() ||
                    !workload->barriers.buffers.empty()) {
                    throw std::runtime_error(
                        "Explicit workload barriers are not supported; declare RenderGraph reads/writes instead");
                }
                if (!workload->textureCopies.empty() ||
                    !workload->bufferToTextureCopies.empty() ||
                    !workload->textureToBufferCopies.empty() ||
                    !workload->textureBlits.empty() ||
                    !workload->mipmapGenerations.empty()) {
                    throw std::runtime_error(
                        "Vulkan texture copy/blit/mipmap workloads are not implemented yet");
                }
                if (!workload->queryResets.empty() ||
                    !workload->timestampWrites.empty() ||
                    !workload->queryResolves.empty()) {
                    throw std::runtime_error(
                        "Vulkan RenderGraph query workloads are not implemented yet");
                }
                if (!workload->indirectDraws.empty() ||
                    !workload->indexedIndirectDraws.empty() ||
                    !workload->indirectDispatches.empty()) {
                    throw std::runtime_error(
                        "Vulkan RenderGraph indirect workloads are not implemented yet");
                }

                for (const RHIBufferCopyDesc& copy : workload->bufferCopies) { // 录制由 RenderGraph 声明过的 buffer-to-buffer copy。
                    const Impl::BufferResource* source = getRenderResource(impl_->buffers, copy.source);
                    const Impl::BufferResource* destination = getRenderResource(impl_->buffers, copy.destination);
                    if (source == nullptr || destination == nullptr ||
                        source->buffer == VK_NULL_HANDLE ||
                        destination->buffer == VK_NULL_HANDLE) {
                        throw std::runtime_error(
                            "Vulkan RenderGraph buffer copy resource is invalid");
                    }
                    if (copy.size == 0 ||
                        copy.sourceOffset > source->desc.size ||
                        copy.size > source->desc.size - copy.sourceOffset ||
                        copy.destinationOffset > destination->desc.size ||
                        copy.size >
                            destination->desc.size - copy.destinationOffset) {
                        throw std::runtime_error(
                            "Vulkan RenderGraph buffer copy range is invalid");
                    }
                    const VkBufferCopy region{
                        copy.sourceOffset, // source VkBuffer 的起始字节。
                        copy.destinationOffset, // destination VkBuffer 的起始字节。
                        copy.size}; // 拷贝字节总数；上方已检查两端范围。
                    vkCmdCopyBuffer(
                        commandBuffer, // 当前 command buffer。
                        source->buffer, // copy source VkBuffer。
                        destination->buffer, // copy destination VkBuffer。
                        1, // regionCount：本 RHI copy 描述映射为一个连续区域。
                        &region); // pRegions：区域起止偏移与大小。
                }
            }

            if (!hasAttachments) {
                // 没有 raster attachment 的 pass 按计算/传输命令录制。图编译器已经验证了
                // pass 类型与命令种类的兼容性；这里仅把 RHI command 映射为 Vulkan 命令。
                for (const RHIDispatchCommand& dispatch : workload->dispatches) { // 无 attachment pass 中的 compute dispatch。
                    const Impl::PipelineResource* pipeline =
                        getRenderResource(impl_->pipelines, dispatch.pipeline);
                    if (pipeline == nullptr || pipeline->pipeline == VK_NULL_HANDLE ||
                        pipeline->bindPoint != VK_PIPELINE_BIND_POINT_COMPUTE) {
                        throw std::runtime_error("RHIDispatchCommand pipeline is invalid");
                    }
                    vkCmdBindPipeline(
                        commandBuffer, // 当前 command buffer。
                        VK_PIPELINE_BIND_POINT_COMPUTE, // 显式选择 compute bind point，与 pipeline 创建时一致。
                        pipeline->pipeline); // 预编译的 VkPipeline。

                    std::vector<VkDescriptorSet> descriptorSets; // 连续 set 编号的 native descriptor set 列表。
                    descriptorSets.reserve(dispatch.bindSets.size()); // 避免按 RHI bind set 数量追加时扩容。
                    for (RHIBindSet bindSetHandle : dispatch.bindSets) {
                        const Impl::BindSetResource* bindSet =
                            getRenderResource(impl_->bindSets, bindSetHandle);
                        if (bindSet == nullptr || bindSet->set == VK_NULL_HANDLE) {
                            throw std::runtime_error(
                                "RHIDispatchCommand bind set is invalid");
                        }
                        descriptorSets.push_back(bindSet->set); // 仅提取 native VkDescriptorSet，保持调用方给出的 set 顺序。
                    }
                    if (!descriptorSets.empty()) {
                        vkCmdBindDescriptorSets(
                            commandBuffer, // 当前 command buffer。
                            VK_PIPELINE_BIND_POINT_COMPUTE, // descriptors 绑定到 compute pipeline 状态。
                            pipeline->layout, // VkPipelineLayout 决定 set layout 和 push-constant layout。
                            0, // firstSet：RHI 约定 bindSets 从 set 0 连续给出。
                            static_cast<u32>(descriptorSets.size()), // descriptor set 数量。
                            descriptorSets.data(), // VkDescriptorSet 数组首地址。
                            0, // dynamicOffsetCount：当前 RHI bind set 暂不传动态 offset。
                            nullptr); // pDynamicOffsets：对应 count 为 0。
                    }
                    vkCmdDispatch(
                        commandBuffer, // 当前 command buffer。
                        dispatch.groupCountX, // X 维 workgroup 数。
                        dispatch.groupCountY, // Y 维 workgroup 数。
                        dispatch.groupCountZ); // Z 维 workgroup 数。
                }
                continue;
            }

            // ExecutionPlan 只缓存 attachment 的整数下标；load/store、clear value 等
            // 动态值仍从当前 packet 的 sourcePass 读取，所以清屏颜色可逐帧变化而不触发
            // RenderGraph 重新编译。
            std::vector<VkRenderingAttachmentInfo> colorAttachments; // Dynamic Rendering 的 color attachment 描述数组。
            std::vector<VkClearValue> colorClearValues; // 保持每个 attachment 独立的 clear union 存储。
            colorAttachments.reserve(compiledPass.colorAttachments.size()); // 避免 attachment 数量已知时反复扩容。
            colorClearValues.reserve(compiledPass.colorAttachments.size()); // clear 数量与 color attachment 数量一一对应。
            for (const RHICompiledRenderGraphAttachment& compiledAttachment :
                 compiledPass.colorAttachments) {
                const RHIRenderGraphAttachmentDesc& attachment =
                    sourcePass.colorAttachments[compiledAttachment.attachmentIndex]; // 当前帧的 load/store/clear 参数。
                const RHITexture texture = graphTextures[compiledAttachment.textureIndex]; // 编译后的 logical texture 映射到实际 image。
                const RHITextureView viewHandle = findViewForTexture(texture, RHITextureAspect::Color); // 找到 color aspect 的 image view。
                const Impl::TextureViewResource* view = getRenderResource(impl_->textureViews, viewHandle); // 解引用成 native VkImageView。
                if (view == nullptr || view->view == VK_NULL_HANDLE) {
                    throw std::runtime_error("RenderGraph color attachment requires a valid texture view");
                }

                // 先保留每个 attachment 对应的转换结果，再把它复制进 attachment 的
                // clearValue；这样多 color attachment 不会复用同一个临时清屏值。
                colorClearValues.push_back(vkClearColor(attachment.clearValue.color)); // 生成并持久保存此 attachment 的 RGBA clear 值。
                VkRenderingAttachmentInfo colorAttachment{}; // 一个 color attachment 的 dynamic-rendering 参数。
                colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO; // Vulkan 结构类型标签。
                colorAttachment.imageView = view->view; // 本 pass 写入/读取的 VkImageView。
                colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // 必须匹配此前 transition 的 color-attachment layout。
                colorAttachment.loadOp = toVkLoadOp(attachment.loadOp); // Load/Clear/DontCare 语义。
                colorAttachment.storeOp = toVkStoreOp(attachment.storeOp); // Store/DontCare 语义，影响 pass 结束后的内容是否保留。
                colorAttachment.clearValue = colorClearValues.back(); // 当 loadOp 为 CLEAR 时使用的值；其他 loadOp 会忽略它。
                colorAttachments.push_back(colorAttachment); // 按颜色 attachment 索引顺序加入 rendering info。
            }

            VkRenderingAttachmentInfo depthAttachment{}; // 可选的 depth/stencil attachment；没有时保持未使用。
            VkClearValue depthClear{}; // depth/stencil attachment 的 clear union，生命周期覆盖 vkCmdBeginRendering。
            if (compiledPass.depthStencilAttachment.has_value()) {
                const RHICompiledRenderGraphAttachment& compiledAttachment =
                    *compiledPass.depthStencilAttachment;
                const RHIRenderGraphAttachmentDesc& attachment =
                    *sourcePass.depthStencilAttachment; // 当前帧 depth/stencil 的 load/store/clear 参数。
                const RHITexture texture = graphTextures[compiledAttachment.textureIndex]; // 取得实际 depth texture。
                const RHITextureView viewHandle = findViewForTexture(texture, RHITextureAspect::Depth); // 获取 depth aspect 的 view。
                const Impl::TextureViewResource* view = getRenderResource(impl_->textureViews, viewHandle); // 取得 native VkImageView。
                if (view == nullptr || view->view == VK_NULL_HANDLE) {
                    throw std::runtime_error("RenderGraph depth attachment requires a valid texture view");
                }

                depthClear = vkClearDepthStencil(attachment.clearValue.depthStencil); // 从 RHI 清屏值转换为 Vulkan union。
                depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO; // Vulkan 结构类型标签。
                depthAttachment.imageView = view->view; // depth/stencil VkImageView。
                depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // 与 transition 后布局一致。
                depthAttachment.loadOp = toVkLoadOp(attachment.loadOp); // 深度/模板的 load 或 clear 语义。
                depthAttachment.storeOp = toVkStoreOp(attachment.storeOp); // pass 后是否保留深度/模板结果。
                depthAttachment.clearValue = depthClear; // 当 loadOp 为 CLEAR 时分别写入 depth/stencil。
            }

            RHIRect2D renderArea = workload == nullptr || // 无 workload 时使用 packet 的全帧 scissor。
                                           workload->scissor.extent.width == 0 ||
                                           workload->scissor.extent.height == 0
                                       ? packet.settings.scissor
                                       : workload->scissor;
            // Dynamic Rendering 不创建 VkRenderPass/VkFramebuffer；这里把一个 RenderGraph
            // raster pass 的 attachment、render area、load/store 直接填入 VkRenderingInfo。
            VkRenderingInfo renderingInfo{}; // vkCmdBeginRendering 的全部 attachment/render area 配置。
            renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO; // Vulkan 结构类型标签。
            renderingInfo.renderArea.offset = {renderArea.offset.x, renderArea.offset.y}; // attachment 中允许 load/store/raster 的左上角像素偏移。
            renderingInfo.renderArea.extent = {renderArea.extent.width, renderArea.extent.height}; // render area 的宽高。
            renderingInfo.layerCount = 1; // 当前 RHI raster pass 仅渲染一个 view layer。
            renderingInfo.colorAttachmentCount = static_cast<u32>(colorAttachments.size()); // color attachment 数量。
            renderingInfo.pColorAttachments = colorAttachments.data(); // 按 location 顺序排列的 color attachment 数组。
            renderingInfo.pDepthAttachment = compiledPass.depthStencilAttachment.has_value() // 无深度附件时必须为 nullptr，Vulkan 不会访问 depthAttachment。
                                                 ? &depthAttachment
                                                 : nullptr;

            vkCmdBeginRendering(commandBuffer, &renderingInfo); // 开始 Dynamic Rendering 范围，后续 draw 写入这些 attachment。

            // workload 可以覆盖全帧默认 viewport/scissor。scissor 同时决定 dynamic rendering
            // 的 renderArea 和 VkScissor，避免 attachment clear 的区域与 rasterization 区域不同。
            RHIViewport viewport = workload == nullptr || // workload 未指定有效 viewport 时退回 packet 默认值。
                                           workload->viewport.width == 0.0F ||
                                           workload->viewport.height == 0.0F
                                       ? packet.settings.viewport
                                       : workload->viewport;
            VkViewport vkViewport{}; // Vulkan dynamic viewport 状态。
            vkViewport.x = viewport.x; // viewport 左边界，单位为 framebuffer 像素。
            vkViewport.y = viewport.y; // viewport 上边界；正/负 height 由上层投影约定配合。
            vkViewport.width = viewport.width; // viewport 水平覆盖宽度。
            vkViewport.height = viewport.height; // viewport 垂直覆盖高度。
            vkViewport.minDepth = viewport.minDepth; // window-space Z 的下限。
            vkViewport.maxDepth = viewport.maxDepth; // window-space Z 的上限。
            vkCmdSetViewport(commandBuffer, 0, 1, &vkViewport); // 写入 slot 0 的一个动态 viewport。

            VkRect2D vkScissor{}; // Vulkan dynamic scissor 状态。
            vkScissor.offset = {renderArea.offset.x, renderArea.offset.y}; // scissor 左上角。
            vkScissor.extent = {renderArea.extent.width, renderArea.extent.height}; // 允许 raster 写入的像素宽高。
            vkCmdSetScissor(commandBuffer, 0, 1, &vkScissor); // 写入 slot 0 的一个动态 scissor。

            const auto recordDraw = [&](const RHIDrawIndexedCommand& draw) { // 统一录制一个 indexed graphics draw。
                // 一个 draw 的最小 Vulkan 绑定集合：pipeline、descriptor sets、vertex buffers、
                // index buffer，然后发出 vkCmdDrawIndexed。
                const Impl::PipelineResource* pipeline = getRenderResource(impl_->pipelines, draw.pipeline); // RHI pipeline -> native VkPipeline/layout。
                if (pipeline == nullptr || pipeline->pipeline == VK_NULL_HANDLE) {
                    throw std::runtime_error("RHIDrawIndexedCommand pipeline is invalid");
                }
                vkCmdBindPipeline(commandBuffer, pipeline->bindPoint, pipeline->pipeline); // 绑定 graphics/compute pipeline；此路径要求 indexed graphics。

                std::vector<VkDescriptorSet> descriptorSets; // 传给 vkCmdBindDescriptorSets 的 native set 数组。
                descriptorSets.reserve(draw.bindSets.size()); // 预留 RHI 描述的 set 数量。
                for (RHIBindSet bindSetHandle : draw.bindSets) { // 按 set 编号顺序解析每个 RHI bind set。
                    const Impl::BindSetResource* bindSet = getRenderResource(impl_->bindSets, bindSetHandle); // 取得 VkDescriptorSet。
                    if (bindSet == nullptr || bindSet->set == VK_NULL_HANDLE) {
                        throw std::runtime_error("RHIDrawIndexedCommand bind set is invalid");
                    }
                    descriptorSets.push_back(bindSet->set); // 保留顺序，set N 对应 pipeline layout 的 set N。
                }
                if (!descriptorSets.empty()) {
                    // RHI 的 bindSets 按 set 编号顺序传入；当前接口从 firstSet=0 连续绑定，
                    // 因此调用方必须提供与 pipeline layout 对齐的集合顺序。
                    vkCmdBindDescriptorSets(
                        commandBuffer, // 当前 command buffer。
                        pipeline->bindPoint, // descriptor set 绑定点与 pipeline 类型保持一致。
                        pipeline->layout, // 定义 descriptor set layout 兼容性。
                        0, // firstSet：从 set 0 开始连续绑定。
                        static_cast<u32>(descriptorSets.size()), // 要绑定的 set 总数。
                        descriptorSets.data(), // VkDescriptorSet 数组。
                        0, // 当前 draw 没有 dynamic descriptor offset。
                        nullptr); // dynamic offset 数组为空。
                }

                for (const RHIVertexStream& stream : draw.vertexStreams) { // 按 RHI binding 编号绑定顶点 buffer。
                    const Impl::BufferResource* vertexBuffer = getRenderResource(impl_->buffers, stream.buffer); // 解引用 VkBuffer。
                    if (vertexBuffer == nullptr || vertexBuffer->buffer == VK_NULL_HANDLE) {
                        throw std::runtime_error("RHIDrawIndexedCommand vertex buffer is invalid");
                    }
                    VkBuffer buffer = vertexBuffer->buffer; // native 顶点 buffer。
                    VkDeviceSize offset = static_cast<VkDeviceSize>(stream.offset); // 该 binding 的首字节偏移。
                    vkCmdBindVertexBuffers(commandBuffer, stream.binding, 1, &buffer, &offset); // 绑定一个 vertex input stream。
                }

                const Impl::BufferResource* indexBuffer = getRenderResource(impl_->buffers, draw.indexStream.buffer); // 取得 index buffer 的 native 资源。
                if (indexBuffer == nullptr || indexBuffer->buffer == VK_NULL_HANDLE) {
                    throw std::runtime_error("RHIDrawIndexedCommand index buffer is invalid");
                }
                const VkIndexType indexType = draw.indexStream.indexType == RHIIndexType::UInt16 // 将 RHI 索引宽度转为 Vulkan 枚举。
                                                  ? VK_INDEX_TYPE_UINT16
                                                  : VK_INDEX_TYPE_UINT32;
                vkCmdBindIndexBuffer(commandBuffer, indexBuffer->buffer, static_cast<VkDeviceSize>(draw.indexStream.offset), indexType); // 绑定 index buffer 和字节偏移。
                vkCmdDrawIndexed(
                    commandBuffer, // 当前 command buffer。
                    draw.indexCount, // 每个 instance 使用的 index 数量。
                    draw.instanceCount, // instance 数量。
                    draw.firstIndex, // index buffer 中的首索引。
                    draw.vertexOffsetElements, // 加到索引结果上的 vertex base offset。
                    draw.firstInstance); // instance ID 的起始值。
            };

            if (workload != nullptr) {
                // 非索引和索引 draw 分开保存，只是为了避免空 index stream；二者共享相同的
                // pipeline -> descriptor sets -> vertex streams 绑定过程。
                for (const RHIDrawCommand& draw : workload->draws) { // 先录制非 indexed draw，保持 workload 原有顺序。
                    const Impl::PipelineResource* pipeline =
                        getRenderResource(impl_->pipelines, draw.pipeline); // 解析 graphics pipeline。
                    if (pipeline == nullptr || pipeline->pipeline == VK_NULL_HANDLE ||
                        pipeline->bindPoint != VK_PIPELINE_BIND_POINT_GRAPHICS) {
                        throw std::runtime_error("RHIDrawCommand pipeline is invalid");
                    }
                    vkCmdBindPipeline(
                        commandBuffer, // 当前 command buffer。
                        VK_PIPELINE_BIND_POINT_GRAPHICS, // 非 indexed draw 必须使用 graphics bind point。
                        pipeline->pipeline); // native VkPipeline。

                    std::vector<VkDescriptorSet> descriptorSets; // 当前 non-indexed draw 的 descriptor set 数组。
                    descriptorSets.reserve(draw.bindSets.size()); // 预留所有 set 元素。
                    for (RHIBindSet bindSetHandle : draw.bindSets) { // 按 set 编号连续解析。
                        const Impl::BindSetResource* bindSet =
                            getRenderResource(impl_->bindSets, bindSetHandle);
                        if (bindSet == nullptr || bindSet->set == VK_NULL_HANDLE) {
                            throw std::runtime_error("RHIDrawCommand bind set is invalid");
                        }
                        descriptorSets.push_back(bindSet->set); // 追加 native VkDescriptorSet。
                    }
                    if (!descriptorSets.empty()) {
                        vkCmdBindDescriptorSets(
                            commandBuffer, // 当前 command buffer。
                            VK_PIPELINE_BIND_POINT_GRAPHICS, // graphics pipeline 的 descriptor bind point。
                            pipeline->layout, // descriptor layout 兼容性检查依据。
                            0, // firstSet：从 set 0 开始。
                            static_cast<u32>(descriptorSets.size()), // set 总数。
                            descriptorSets.data(), // set 数组。
                            0, // dynamicOffsetCount：无动态 offset。
                            nullptr); // pDynamicOffsets：无动态 offset 数据。
                    }
                    for (const RHIVertexStream& stream : draw.vertexStreams) { // 绑定 non-indexed draw 的各顶点流。
                        const Impl::BufferResource* vertexBuffer =
                            getRenderResource(impl_->buffers, stream.buffer);
                        if (vertexBuffer == nullptr ||
                            vertexBuffer->buffer == VK_NULL_HANDLE) {
                            throw std::runtime_error(
                                "RHIDrawCommand vertex buffer is invalid");
                        }
                        const VkBuffer buffer = vertexBuffer->buffer; // native VkBuffer。
                        const VkDeviceSize offset = static_cast<VkDeviceSize>(stream.offset); // binding 起始字节。
                        vkCmdBindVertexBuffers(
                            commandBuffer, // 当前 command buffer。
                            stream.binding, // vertex input binding 编号。
                            1, // bindingCount：一次绑定一个 stream。
                            &buffer, // VkBuffer 数组首地址。
                            &offset); // 每个 buffer 对应的字节偏移。
                    }
                    vkCmdDraw(
                        commandBuffer, // 当前 command buffer。
                        draw.vertexCount, // 每个 instance 的顶点数量。
                        draw.instanceCount, // instance 数量。
                        draw.firstVertex, // vertex buffer 中的起始顶点。
                        draw.firstInstance); // instance ID 起始值。
                }
                for (const RHIDrawIndexedCommand& draw : workload->indexedDraws) {
                    recordDraw(draw);
                }
            }

            vkCmdEndRendering(commandBuffer); // 结束 Dynamic Rendering 范围；之后不能继续写入这些 attachment。
        }

        if (packet.present.has_value()) { // 有 present 请求时，在提交前把 swapchain image 转回 present layout。
            // Present 不在 command buffer 中执行，但调用 vkQueuePresentKHR 前，swapchain image
            // 必须从最后一次 color attachment 写入转换回 PRESENT_SRC_KHR。
            const Impl::SwapchainResource* swapchain = getRenderResource(impl_->swapchains, packet.present->swapchain); // 解析 swapchain 及其 image 数组。
            if (swapchain != nullptr && packet.present->imageIndex < swapchain->images.size()) {
                transitionTexture(swapchain->images[packet.present->imageIndex], RHIResourceState::Present); // COLOR_ATTACHMENT -> PRESENT_SRC_KHR。
            }
        }

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) { // 结束录制并让驱动完成 command buffer 验证。
            throw std::runtime_error("vkEndCommandBuffer failed");
        }

        // packet 中的 waits/signals 描述外部 GPU 工作依赖；RenderGraph 内部 pass 当前录在
        // 同一 command buffer，天然按命令顺序执行。这里把外部同步值组装到一次 queue submit。
        std::vector<VkSemaphore> waitSignals; // 本次 submit 需要等待的 binary/timeline semaphore。
        std::vector<VkPipelineStageFlags> waitStages; // 每个 wait semaphore 开始阻塞的 pipeline stage。
        std::vector<VkSemaphore> signalSemaphores; // 本次 submit 完成后 signal 的 semaphore。
        std::vector<u64> waitValues; // 与 waitSignals 一一对应的 timeline value；binary 项为占位值。
        std::vector<u64> signalValues; // 与 signalSemaphores 一一对应的 timeline signal value。
        bool usesTimelineSemaphore = false; // pNext 是否需要 VkTimelineSemaphoreSubmitInfo。

        for (const RHIQueueSubmitDesc& submitDesc : packet.submissions) { // 汇总 packet 中的逻辑 submission 描述。
            // 一个 packet 可以包含多个逻辑 submission 描述；当前实现将它们的 wait/signal
            // 汇总为一次 graphics queue submit，passNames 的顺序已在上层验证。
            for (const RHIQueueWaitDesc& wait : submitDesc.waits) { // 处理当前逻辑 submission 的 GPU wait。
                const Impl::GPUWaitGPUSignalResource* semaphore = getRenderResource(impl_->gpuWaitGPUSignals, wait.signal); // RHI signal -> VkSemaphore。
                if (semaphore == nullptr || semaphore->semaphore == VK_NULL_HANDLE) {
                    throw std::runtime_error("RHIFramePacket submission contains an invalid wait semaphore");
                }
                usesTimelineSemaphore = usesTimelineSemaphore || semaphore->desc.type == RHIGPUWaitGPUSignalType::Timeline; // 任何 timeline wait 都启用 pNext。
                waitSignals.push_back(semaphore->semaphore); // VkSubmitInfo::pWaitSemaphores 元素。
                waitStages.push_back(toVkPipelineStages(wait.stages)); // wait 对应的 destination stage mask。
                waitValues.push_back(wait.value); // timeline wait value；binary semaphore 此值会被 Vulkan 忽略。
            }
            for (const RHIQueueSignalDesc& signal : submitDesc.signals) { // 处理当前逻辑 submission 的 GPU signal。
                const Impl::GPUWaitGPUSignalResource* semaphore = getRenderResource(impl_->gpuWaitGPUSignals, signal.signal); // RHI signal -> VkSemaphore。
                if (semaphore == nullptr || semaphore->semaphore == VK_NULL_HANDLE) {
                    throw std::runtime_error("RHIFramePacket submission contains an invalid signal semaphore");
                }
                usesTimelineSemaphore = usesTimelineSemaphore || semaphore->desc.type == RHIGPUWaitGPUSignalType::Timeline; // 任何 timeline signal 都启用 pNext。
                signalSemaphores.push_back(semaphore->semaphore); // VkSubmitInfo::pSignalSemaphores 元素。
                signalValues.push_back(signal.value); // timeline signal value；binary semaphore 此值为占位。
            }
        }

        // 每次图形提交都在同一个 timeline semaphore 上 signal 一个严格递增值。
        // FrameContext 只保存自己对应的值，因此一个 semaphore 就能替代 N 个逐帧 fence。
        const u64 frameCompletionValue = impl_->lastSubmissionSerial + 1; // 本次提交在设备 timeline 上唯一且递增的完成序号。
        signalSemaphores.push_back(impl_->frameTimelineSemaphore); // 每帧都 signal 内部 timeline，用于槽位和 deferred release 回收。
        signalValues.push_back(frameCompletionValue); // 与内部 frameTimelineSemaphore 对应的 value。
        usesTimelineSemaphore = true; // 内部 timeline 存在，必须挂接 VkTimelineSemaphoreSubmitInfo。

        VkTimelineSemaphoreSubmitInfo timelineInfo{}; // timeline semaphore 的 value 数组扩展结构。
        timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO; // Vulkan 结构类型标签。
        timelineInfo.waitSemaphoreValueCount = static_cast<u32>(waitValues.size()); // 必须与 waitSemaphoreCount 相同。
        timelineInfo.pWaitSemaphoreValues = waitValues.data(); // 每个 wait semaphore 的对应 value。
        timelineInfo.signalSemaphoreValueCount = static_cast<u32>(signalValues.size()); // 必须与 signalSemaphoreCount 相同。
        timelineInfo.pSignalSemaphoreValues = signalValues.data(); // 每个 signal semaphore 的对应 value。

        // pNext 链只在存在 timeline semaphore 时才需要。Binary semaphore 对应的 value 会被
        // Vulkan 忽略，但为了结构对齐仍在 waitValues/signalValues 中保留一个位置。
        VkSubmitInfo SubmitInfo{}; // 一次 vkQueueSubmit 的顶层描述。
        SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO; // Vulkan 结构类型标签。
        SubmitInfo.pNext = usesTimelineSemaphore ? &timelineInfo : nullptr; // 有 timeline 时提供 value 扩展。
        SubmitInfo.waitSemaphoreCount = static_cast<u32>(waitSignals.size()); // wait semaphore 个数。
        SubmitInfo.pWaitSemaphores = waitSignals.data(); // wait semaphore 数组。
        SubmitInfo.pWaitDstStageMask = waitStages.data(); // 与 wait semaphore 一一对应的阻塞阶段。
        SubmitInfo.commandBufferCount = 1; // 当前实现把一帧录制在一个 command buffer。
        SubmitInfo.pCommandBuffers = &commandBuffer; // 要提交的 command buffer 地址。
        SubmitInfo.signalSemaphoreCount = static_cast<u32>(signalSemaphores.size()); // signal semaphore 个数。
        SubmitInfo.pSignalSemaphores = signalSemaphores.data(); // signal semaphore 数组。

        if (vkQueueSubmit(
                impl_->native.graphicsQueue, // 目标 graphics queue；本后端当前把整帧命令提交到此队列。
                1, // submitCount：一次提交一个 VkSubmitInfo。
                &SubmitInfo, // 指向上方已填充的提交描述。
                VK_NULL_HANDLE) != VK_SUCCESS) { // 不使用传统 VkFence，完成状态由 frame timeline 跟踪。
            throw std::runtime_error("vkQueueSubmit(recorded frame) failed");
        }
        frameSubmitted = true; // 从此处开始，GPU 可能异步读取 staging/transient 资源。
        // 只有 vkQueueSubmit 成功后，staging 与 transient native 对象才可能仍被 GPU 使用。
        // 从此刻起它们的释放由 frameCompletionValue 和延迟回收队列保证安全。
        impl_->deviceKnownIdle = false; // 队列已有未完成工作，不能把 device 当作空闲。
        frame->prepared = false; // 当前 FrameContext 已经提交，不再是可直接录制的 prepared 状态。
        frame->completionValue = frameCompletionValue; // 槽位复用前必须等待的 timeline value。
        impl_->lastSubmissionSerial = frameCompletionValue; // 更新全局最近一次提交序号。
        impl_->nextFrameContext =
            (impl_->nextFrameContext + 1) % static_cast<u32>(impl_->frameContexts.size()); // 轮转到下一个 frames-in-flight 槽位。
        // completionValue 已写入 FrameContext，此后 Destroy 会把 native 资源挂到对应
        // serial 的延迟回收队列，直到 frame timeline 到达该值才真正释放。
        releaseTransientResources(); // Destroy 现在会看到 completion serial，并转入延迟回收队列。

        if (packet.present.has_value()) {
            return Present(*packet.present, errorMessage); // 提交已完成后执行 acquire/present 的 RHI 封装逻辑。
        }
        return true; // 没有 present 时，成功提交 command buffer 即表示本函数成功。
    } catch (const std::exception& error) { // 将 Vulkan/RHI 异常统一转换为 bool + errorMessage API。
        // 命令录制阶段失败时尚未提交，staging/transient 可以立即释放；若提交已经成功，
        // releaseTransientResources 会转入 deferRelease，不能直接销毁 GPU 仍可能访问的对象。
        releaseTransientResources(); // 未提交时立即释放；已提交时内部延迟释放，避免 GPU use-after-free。
        if (frame != nullptr && !frameSubmitted) { // 只有 queue submit 失败/录制失败时才能直接清空 staging。
            impl_->releaseStagingResources(*frame); // 释放本帧已经创建但不会再被 GPU 读取的 staging。
            frame->prepared = false; // 允许下次调用重新选择并准备此槽位。
        }
        if (errorMessage != nullptr) {
            *errorMessage = error.what(); // 把异常消息交还给 RHI 上层。
        }
        return false; // 当前帧失败，不让异常越过 RHI C++ API 边界。
    }
}

// 便捷重载：调用方没有缓存 execution plan 时，在提交前完成一次 RenderGraph 编译。
bool RHIVulkan::SubmitFrame(const RHIFramePacket& packet, std::string* errorMessage) {
    // 这个重载面向直接使用 RHIVulkan 的调用方：先把声明式 graph 编译为可执行计划，再交给
    // 下面的计划重载录制。通过 RHIDevice 调用时，设备层还会缓存结构相同的计划。
    RHIRenderGraphCompileResult graph =
        CompileRHIRenderGraph(packet.graph, packet.workloads); // 生成拓扑排序、生命周期和 transition 计划。
    if (!graph.Succeeded()) {
        if (errorMessage != nullptr) {
            *errorMessage = graph.ErrorMessage();
        }
        return false;
    }
    return SubmitFrame(packet, graph.plan, errorMessage); // 复用带 plan 的主重载，避免两份录制逻辑。
}

// 带缓存 plan 的重载：先验证用户自定义 submission，再决定走完整 command-buffer 路径还是低层提交路径。
bool RHIVulkan::SubmitFrame(
    const RHIFramePacket& packet, // 当前帧 packet。
    const RHIRenderGraphExecutionPlan& graphPlan, // 已缓存/预编译的 execution plan。
    std::string* errorMessage) { // 可选错误输出。
    // 自定义 submissions 可以按 passName 表达提交范围。执行前验证它们没有打乱 compiled
    // dependency 顺序；空 submissions 则让本后端把整帧合并成一次图形队列提交。
    if (!ValidateRHIRenderGraphSubmissions( // 检查 passName 范围和依赖顺序是否仍然有效。
            packet.graph,
            graphPlan,
            packet.submissions,
            errorMessage)) {
        return false;
    }
    if (!graphPlan.passes.empty() || !packet.uploads.buffers.empty() ||
        !packet.uploads.textures.empty()) {
        // 只要有 RenderGraph work 或 upload，就必须建立 command buffer；完全没有 GPU work
        // 的 packet 才走低层 Submit/Present，用于单独测试外部同步或呈现流程。
        return RecordAndSubmitFrame(packet, graphPlan, errorMessage); // 有 GPU work 时走完整 Vulkan 录制路径。
    }

    for (const RHIQueueSubmitDesc& submitDesc : packet.submissions) { // 无 graph/upload 时复用低层 semaphore submit。
        if (!Submit(submitDesc, errorMessage)) { // 低层 Submit 负责仅提交外部 wait/signal，不创建 command buffer。
            return false;
        }
    }
    if (packet.present.has_value()) { // 空 GPU work packet 仍可单独执行 present。
        return Present(*packet.present, errorMessage); // 调用 RHI 的交换链呈现封装。
    }
    return true;
}

// 等待设备完全空闲，并把所有按 serial 延迟的资源释放推进到完成状态。
void RHIVulkan::WaitIdle() const noexcept {
    if (IsInitialized()) {
        // WaitIdle 是 resize、退出或大规模资源重建的同步边界。它牺牲并行度换取“所有已提交
        // 工作都完成”的强保证，因此可以安全释放所有 staging 与 deferred native 资源。
        vkDeviceWaitIdle(impl_->native.device); // 阻塞 CPU，直到 device 上所有 queue work 完成。
        impl_->completedSubmissionSerial = impl_->lastSubmissionSerial; // 所有已提交 serial 均确认完成。
        impl_->hasUntrackedSubmissions = false; // 不再有未纳入当前 serial 跟踪的工作。
        impl_->deviceKnownIdle = true; // 后续 Destroy 可安全立即释放 native 资源。
        for (Impl::FrameContext& frame : impl_->frameContexts) { // 清空每个 frames-in-flight 槽的 staging。
            impl_->releaseStagingResources(frame); // GPU 已空闲，所有 staging 都不再被访问。
        }
        impl_->flushDeferredReleases(); // 立即执行此前挂起的 buffer/image/view/pipeline 等释放。
    }
}

// Destroy 会先清空句柄槽，再按 submission serial 延迟释放 native 对象。

} // namespace RHI













