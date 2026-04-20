/*
 * Copyright (c) 2023 - 2026 Rachit
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "RenderPass.h"

#include "../../../Shared/Debug/GenerateDrawCalls.h"
#include "Debug/AABB.h"
#include "Vulkan/DebugUtils.h"

namespace Renderer::Debug
{
    RenderPass::RenderPass
    (
        VkDevice device,
        VmaAllocator allocator,
        const Vk::Swapchain& swapchain,
        Vk::PipelineManager& pipelineManager,
        Vk::StagingPool& stagingPool
    )
    {
        pipelineManager.AddPipeline("Debug/AABB/GenerateDrawCalls", Vk::PipelineConfig{}
            .SetPipelineType(VK_PIPELINE_BIND_POINT_COMPUTE)
            .AttachShader("Debug/GenerateDrawCalls.comp", VK_SHADER_STAGE_COMPUTE_BIT)
            .AddPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Generate::Constants))
        );

        constexpr std::array DYNAMIC_STATES =
        {
            VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
            VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT
        };

        const std::array colorFormats = {swapchain.surfaceFormat.format};

        pipelineManager.AddPipeline("Debug/AABB", Vk::PipelineConfig{}
            .SetPipelineType(VK_PIPELINE_BIND_POINT_GRAPHICS)
            .SetRenderingInfo(0, colorFormats, VK_FORMAT_UNDEFINED)
            .AttachShader("Debug/AABB.vert", VK_SHADER_STAGE_VERTEX_BIT)
            .AttachShader("Debug/AABB.frag", VK_SHADER_STAGE_FRAGMENT_BIT)
            .SetDynamicStates(DYNAMIC_STATES)
            .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
            .SetRasterizerState(VK_FALSE, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL)
            .SetDepthState(VK_FALSE, VK_FALSE, VK_COMPARE_OP_ALWAYS)
            .AddDefaultBlendAttachment()
            .AddPushConstant(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(AABB::Constants))
        );

        constexpr std::array<u32, 24> AABB_INDICES = {0, 1, 1, 3, 3, 2, 2, 0, 4, 5, 5, 7, 7, 6, 6, 4, 0, 4, 1, 5, 2, 6, 3, 7};

        constexpr VkDeviceSize INDICES_SIZE = sizeof(u32) * AABB_INDICES.size();

        m_indexBuffer = Vk::Buffer
        (
            allocator,
            INDICES_SIZE,
            0,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        );

        constexpr usize DRAW_CALL_BUFFER_COUNT = 4;

        m_drawCallBuffer = Vk::Buffer
        (
            allocator,
            DRAW_CALL_BUFFER_COUNT * sizeof(VkDrawIndexedIndirectCommand),
            0,
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        );

        m_drawCallBuffer.GetDeviceAddress(device);

        m_pendingIndexUpload = stagingPool.Allocate
        (
            device,
            allocator,
            INDICES_SIZE,
            0
        );

        std::memcpy(m_pendingIndexUpload->hostAddress, AABB_INDICES.data(), INDICES_SIZE);

        Vk::SetDebugName(device, m_indexBuffer.handle,    "Debug/AABB/IndexBuffer");
        Vk::SetDebugName(device, m_drawCallBuffer.handle, "Debug/AABB/DrawCalls");
    }

    void RenderPass::Render
    (
        usize FIF,
        usize frameIndex,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::Swapchain& swapchain,
        const Buffers::SceneBuffer& sceneBuffer,
        const Buffers::MeshBuffer& meshBuffer,
        const Buffers::IndirectBuffer& indirectBuffer,
        Vk::StagingPool& stagingPool,
        Util::DeletionQueue& deletionQueue
    )
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Renderer"))
            {
                if (ImGui::CollapsingHeader("Debug Renderer"))
                {
                    ImGui::Checkbox("Render AABBs", &m_renderAABBDebug);
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        Vk::BeginLabel(cmdBuffer, "Debug/AABB", {0.6117f, 0.5749f, 0.1901f, 1.0f});

        if (m_pendingIndexUpload.has_value())
        {
            Vk::BeginLabel(cmdBuffer, "AABB Index Transfer", {0.6117f, 0.0749f, 0.3901f, 1.0f});

            const VkBufferCopy2 copyRegion =
            {
                .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                .pNext     = nullptr,
                .srcOffset = m_pendingIndexUpload->memoryBlock.offset,
                .dstOffset = 0,
                .size      = m_pendingIndexUpload->memoryBlock.size
            };

            const VkCopyBufferInfo2 copyInfo =
            {
                .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
                .pNext       = nullptr,
                .srcBuffer   = m_pendingIndexUpload->buffer,
                .dstBuffer   = m_indexBuffer.handle,
                .regionCount = 1,
                .pRegions    = &copyRegion
            };

            vkCmdCopyBuffer2(cmdBuffer.handle, &copyInfo);

            m_indexBuffer.Barrier
            (
                cmdBuffer,
                Vk::BufferBarrier{
                    .srcStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                    .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .dstStageMask   = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
                    .dstAccessMask  = VK_ACCESS_2_INDEX_READ_BIT,
                    .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .offset         = 0,
                    .size           = m_pendingIndexUpload->memoryBlock.size
                }
            );

            deletionQueue.PushDeletor([&stagingPool, stagingMemoryBlock = m_pendingIndexUpload.value()] () mutable
            {
                stagingPool.Free(stagingMemoryBlock);
            });

            m_pendingIndexUpload = std::nullopt;

            Vk::EndLabel(cmdBuffer);
        }

        if (m_renderAABBDebug)
        {
            GenerateAABBDrawCalls
            (
                cmdBuffer,
                pipelineManager,
                indirectBuffer
            );

            RenderDebugAABB
            (
                FIF,
                frameIndex,
                cmdBuffer,
                pipelineManager,
                swapchain,
                sceneBuffer,
                meshBuffer,
                indirectBuffer
            );
        }

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::GenerateAABBDrawCalls
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Buffers::IndirectBuffer& indirectBuffer
    )
    {
        Vk::BeginLabel(cmdBuffer, "Generate Draw Calls", {0.1657f, 0.5149f, 0.4901f, 1.0f});

        Vk::BarrierWriter barrierWriter = {};

        barrierWriter
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.opaqueBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .srcAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = sizeof(u32)
            }
        )
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.opaqueDoubleSidedBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .srcAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = sizeof(u32)
            }
        )
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.alphaMaskedBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .srcAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = sizeof(u32)
            }
        )
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .srcAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = sizeof(u32)
            }
        )
        .WriteBufferBarrier(
            m_drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .srcAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = m_drawCallBuffer.size
            }
        )
        .Execute(cmdBuffer);

        const auto& pipeline = pipelineManager.GetPipeline("Debug/AABB/GenerateDrawCalls");

        pipeline.Bind(cmdBuffer);

        const Generate::Constants constants =
        {
            .CulledOpaqueDrawCalls                 = indirectBuffer.frustumCulledBuffers.opaqueBuffer.drawCallBuffer.deviceAddress,
            .CulledOpaqueDoubleSidedDrawCalls      = indirectBuffer.frustumCulledBuffers.opaqueDoubleSidedBuffer.drawCallBuffer.deviceAddress,
            .CulledAlphaMaskedDrawCalls            = indirectBuffer.frustumCulledBuffers.alphaMaskedBuffer.drawCallBuffer.deviceAddress,
            .CulledAlphaMaskedDoubleSidedDrawCalls = indirectBuffer.frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.drawCallBuffer.deviceAddress,
            .DebugAABBDrawCalls                    = m_drawCallBuffer.deviceAddress
        };

        pipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_COMPUTE_BIT,
            constants
        );

        vkCmdDispatch
        (
            cmdBuffer.handle,
            1,
            1,
            1
        );

        barrierWriter
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.opaqueBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .dstAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = sizeof(u32)
            }
        )
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.opaqueDoubleSidedBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .dstAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = sizeof(u32)
            }
        )
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.alphaMaskedBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .dstAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = sizeof(u32)
            }
        )
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .dstAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = sizeof(u32)
            }
        )
        .WriteBufferBarrier(
            m_drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .dstAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = m_drawCallBuffer.size
            }
        )
        .Execute(cmdBuffer);

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::RenderDebugAABB
    (
        usize FIF,
        usize frameIndex,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::Swapchain& swapchain,
        const Buffers::SceneBuffer& sceneBuffer,
        const Buffers::MeshBuffer& meshBuffer,
        const Buffers::IndirectBuffer& indirectBuffer
    )
    {
        Vk::BeginLabel(cmdBuffer, "Render AABB Debug", {0.1657f, 0.9149f, 0.4901f, 1.0f});

        const auto& pipeline = pipelineManager.GetPipeline("Debug/AABB");

        const auto& currentImageView = swapchain.imageViews[swapchain.imageIndex];

        const VkRenderingAttachmentInfo colorAttachmentInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = currentImageView.handle,
            .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode        = VK_RESOLVE_MODE_NONE,
            .resolveImageView   = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp             = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue         = {}
        };

        const VkRenderingInfo renderInfo =
        {
            .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext                = nullptr,
            .flags                = 0,
            .renderArea           = {
                .offset = {.x = 0, .y = 0},
                .extent = swapchain.extent
            },
            .layerCount           = 1,
            .viewMask             = 0,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &colorAttachmentInfo,
            .pDepthAttachment     = nullptr,
            .pStencilAttachment   = nullptr
        };

        vkCmdBeginRendering(cmdBuffer.handle, &renderInfo);

        pipeline.Bind(cmdBuffer);

        const VkViewport viewport =
        {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<f32>(swapchain.extent.width),
            .height   = static_cast<f32>(swapchain.extent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vkCmdSetViewportWithCount(cmdBuffer.handle, 1, &viewport);

        const VkRect2D scissor =
        {
            .offset = {.x = 0, .y = 0},
            .extent = swapchain.extent
        };

        vkCmdSetScissorWithCount(cmdBuffer.handle, 1, &scissor);

        vkCmdBindIndexBuffer
        (
            cmdBuffer.handle,
            m_indexBuffer.handle,
            0,
            VK_INDEX_TYPE_UINT32
        );

        // Opaque
        {
            Vk::BeginLabel(cmdBuffer, "Opaque", glm::vec4(0.6091f, 0.7243f, 0.2549f, 1.0f));

            // Single-Sided
            {
                Vk::BeginLabel(cmdBuffer, "Single Sided", glm::vec4(0.3091f, 0.7243f, 0.2549f, 1.0f));

                const auto constants = AABB::Constants
                {
                    .Scene           = sceneBuffer.graphicsBuffers.sceneBuffers[FIF].deviceAddress,
                    .Meshes          = meshBuffer.GetCurrentMeshBuffer(frameIndex).deviceAddress,
                    .Instances       = meshBuffer.GetCurrentInstanceBuffer(frameIndex).deviceAddress,
                    .InstanceIndices = indirectBuffer.frustumCulledBuffers.opaqueBuffer.instanceIndexBuffer.deviceAddress
                };

                pipeline.PushConstants
                (
                   cmdBuffer,
                   VK_SHADER_STAGE_VERTEX_BIT,
                   constants
                );

                vkCmdDrawIndexedIndirect
                (
                    cmdBuffer.handle,
                    m_drawCallBuffer.handle,
                    0 * sizeof(VkDrawIndexedIndirectCommand),
                    1,
                    sizeof(VkDrawIndexedIndirectCommand)
                );

                Vk::EndLabel(cmdBuffer);
            }

            // Double Sided
            {
                Vk::BeginLabel(cmdBuffer, "Double Sided", glm::vec4(0.6091f, 0.2213f, 0.2549f, 1.0f));

                const auto constants = AABB::Constants
                {
                    .Scene           = sceneBuffer.graphicsBuffers.sceneBuffers[FIF].deviceAddress,
                    .Meshes          = meshBuffer.GetCurrentMeshBuffer(frameIndex).deviceAddress,
                    .Instances       = meshBuffer.GetCurrentInstanceBuffer(frameIndex).deviceAddress,
                    .InstanceIndices = indirectBuffer.frustumCulledBuffers.opaqueDoubleSidedBuffer.instanceIndexBuffer.deviceAddress,
                };

                pipeline.PushConstants
                (
                   cmdBuffer,
                   VK_SHADER_STAGE_VERTEX_BIT,
                   constants
                );

                vkCmdDrawIndexedIndirect
                (
                    cmdBuffer.handle,
                    m_drawCallBuffer.handle,
                    1 * sizeof(VkDrawIndexedIndirectCommand),
                    1,
                    sizeof(VkDrawIndexedIndirectCommand)
                );

                Vk::EndLabel(cmdBuffer);
            }

            Vk::EndLabel(cmdBuffer);
        }

        // Alpha Masked
        {
            Vk::BeginLabel(cmdBuffer, "Alpha Masked", glm::vec4(0.9091f, 0.2243f, 0.6549f, 1.0f));

            // Single-Sided
            {
                Vk::BeginLabel(cmdBuffer, "Single Sided", glm::vec4(0.3091f, 0.7243f, 0.2549f, 1.0f));

                const auto constants = AABB::Constants
                {
                    .Scene               = sceneBuffer.graphicsBuffers.sceneBuffers[FIF].deviceAddress,
                    .Meshes              = meshBuffer.GetCurrentMeshBuffer(frameIndex).deviceAddress,
                    .Instances           = meshBuffer.GetCurrentInstanceBuffer(frameIndex).deviceAddress,
                    .InstanceIndices     = indirectBuffer.frustumCulledBuffers.alphaMaskedBuffer.instanceIndexBuffer.deviceAddress
                };

                pipeline.PushConstants
                (
                   cmdBuffer,
                   VK_SHADER_STAGE_VERTEX_BIT,
                   constants
                );

                vkCmdDrawIndexedIndirect
                (
                    cmdBuffer.handle,
                    m_drawCallBuffer.handle,
                    2 * sizeof(VkDrawIndexedIndirectCommand),
                    1,
                    sizeof(VkDrawIndexedIndirectCommand)
                );

                Vk::EndLabel(cmdBuffer);
            }

            // Double Sided
            {
                Vk::BeginLabel(cmdBuffer, "Double Sided", glm::vec4(0.6091f, 0.2213f, 0.2549f, 1.0f));

                const auto constants = AABB::Constants
                {
                    .Scene               = sceneBuffer.graphicsBuffers.sceneBuffers[FIF].deviceAddress,
                    .Meshes              = meshBuffer.GetCurrentMeshBuffer(frameIndex).deviceAddress,
                    .Instances           = meshBuffer.GetCurrentInstanceBuffer(frameIndex).deviceAddress,
                    .InstanceIndices     = indirectBuffer.frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.instanceIndexBuffer.deviceAddress
                };

                pipeline.PushConstants
                (
                   cmdBuffer,
                   VK_SHADER_STAGE_VERTEX_BIT,
                   constants
                );

                vkCmdDrawIndexedIndirect
                (
                    cmdBuffer.handle,
                    m_drawCallBuffer.handle,
                    3 * sizeof(VkDrawIndexedIndirectCommand),
                    1,
                    sizeof(VkDrawIndexedIndirectCommand)
                );

                Vk::EndLabel(cmdBuffer);
            }

            Vk::EndLabel(cmdBuffer);
        }

        vkCmdEndRendering(cmdBuffer.handle);

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::Destroy(VmaAllocator allocator, Vk::StagingPool& stagingPool)
    {
        m_indexBuffer.Destroy(allocator);
        m_drawCallBuffer.Destroy(allocator);

        if (m_pendingIndexUpload.has_value())
        {
            stagingPool.Free(m_pendingIndexUpload.value());

            m_pendingIndexUpload = std::nullopt;
        }
    }
}
