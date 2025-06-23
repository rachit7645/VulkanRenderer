/*
 * Copyright (c) 2023 - 2025 Rachit
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

#include "Dispatch.h"

#include "Vulkan/DebugUtils.h"
#include "Culling/Frustum.h"

namespace Renderer::Culling
{
    constexpr auto CULLING_WORKGROUP_SIZE = 64;

    Dispatch::Dispatch
    (
        VkDevice device,
        VmaAllocator allocator,
        Vk::PipelineManager& pipelineManager
    )
        : m_frustumBuffer(device, allocator)
    {
        pipelineManager.AddPipeline("Culling/Frustum", Vk::PipelineConfig{}
            .SetPipelineType(VK_PIPELINE_BIND_POINT_COMPUTE)
            .AttachShader("Culling/Frustum.comp", VK_SHADER_STAGE_COMPUTE_BIT)
            .AddPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Frustum::Constants))
        );
    }

    void Dispatch::Frustum
    (
        usize FIF,
        usize frameIndex,
        const glm::mat4& projectionView,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Buffers::MeshBuffer& meshBuffer,
        const Buffers::IndirectBuffer& indirectBuffer
    )
    {
        Vk::BeginLabel(cmdBuffer, "Frustum Culling", glm::vec4(0.6196f, 0.5588f, 0.8588f, 1.0f));

        if (!NeedsDispatch(FIF, cmdBuffer, indirectBuffer))
        {
            Vk::EndLabel(cmdBuffer);

            return;
        }

        PreDispatch
        (
            FIF,
            projectionView,
            cmdBuffer,
            indirectBuffer
        );

        const auto& frustumPipeline = pipelineManager.GetPipeline("Culling/Frustum");

        frustumPipeline.Bind(cmdBuffer);

        const auto constants = Frustum::Constants
        {
            .Meshes                                  = meshBuffer.GetCurrentBuffer(frameIndex).deviceAddress,
            .DrawCalls                               = indirectBuffer.writtenDrawCallBuffers[FIF].drawCallBuffer.deviceAddress,
            .CulledOpaqueDrawCalls                   = indirectBuffer.frustumCulledBuffers.opaqueBuffer.drawCallBuffer.deviceAddress,
            .CulledOpaqueMeshIndices                 = indirectBuffer.frustumCulledBuffers.opaqueBuffer.meshIndexBuffer->deviceAddress,
            .CulledOpaqueDoubleSidedDrawCalls        = indirectBuffer.frustumCulledBuffers.opaqueDoubleSidedBuffer.drawCallBuffer.deviceAddress,
            .CulledOpaqueDoubleSidedMeshIndices      = indirectBuffer.frustumCulledBuffers.opaqueDoubleSidedBuffer.meshIndexBuffer->deviceAddress,
            .CulledAlphaMaskedDrawCalls              = indirectBuffer.frustumCulledBuffers.alphaMaskedBuffer.drawCallBuffer.deviceAddress,
            .CulledAlphaMaskedMeshIndices            = indirectBuffer.frustumCulledBuffers.alphaMaskedBuffer.meshIndexBuffer->deviceAddress,
            .CulledAlphaMaskedDoubleSidedDrawCalls   = indirectBuffer.frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.drawCallBuffer.deviceAddress,
            .CulledAlphaMaskedDoubleSidedMeshIndices = indirectBuffer.frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.meshIndexBuffer->deviceAddress,
            .Frustum                                 = m_frustumBuffer.buffer.deviceAddress
        };

        frustumPipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_COMPUTE_BIT,
            constants
        );

        Execute(FIF, cmdBuffer, indirectBuffer);

        PostDispatch(FIF, cmdBuffer, indirectBuffer);

        Vk::EndLabel(cmdBuffer);
    }

    bool Dispatch::NeedsDispatch
    (
        usize FIF,
        const Vk::CommandBuffer& cmdBuffer,
        const Buffers::IndirectBuffer& indirectBuffer
    )
    {
        const u32 drawCallCount = indirectBuffer.writtenDrawCallBuffers[FIF].writtenDrawCount;

        if (drawCallCount != 0)
        {
            return true;
        }

        m_barrierWriter
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.opaqueBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .srcAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .dstAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
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
                .dstStageMask   = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .dstAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
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
                .dstStageMask   = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .dstAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
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
                .dstStageMask   = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .dstAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = sizeof(u32)
            }
        )
        .Execute(cmdBuffer);

        constexpr u32 ZERO = 0;

        vkCmdUpdateBuffer
        (
            cmdBuffer.handle,
            indirectBuffer.frustumCulledBuffers.opaqueBuffer.drawCallBuffer.handle,
            0,
            sizeof(u32),
            &ZERO
        );

        vkCmdUpdateBuffer
        (
            cmdBuffer.handle,
            indirectBuffer.frustumCulledBuffers.opaqueDoubleSidedBuffer.drawCallBuffer.handle,
            0,
            sizeof(u32),
            &ZERO
        );

        vkCmdUpdateBuffer
        (
            cmdBuffer.handle,
            indirectBuffer.frustumCulledBuffers.alphaMaskedBuffer.drawCallBuffer.handle,
            0,
            sizeof(u32),
            &ZERO
        );

        vkCmdUpdateBuffer
        (
            cmdBuffer.handle,
            indirectBuffer.frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.drawCallBuffer.handle,
            0,
            sizeof(u32),
            &ZERO
        );

        m_barrierWriter
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.opaqueBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
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
                .srcStageMask   = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
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
                .srcStageMask   = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
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
                .srcStageMask   = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .dstAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = sizeof(u32)
            }
        )
        .Execute(cmdBuffer);

        return false;
    }

    void Dispatch::PreDispatch
    (
        usize FIF,
        const glm::mat4& projectionView,
        const Vk::CommandBuffer& cmdBuffer,
        const Buffers::IndirectBuffer& indirectBuffer
    )
    {
        m_frustumBuffer.Load(cmdBuffer, projectionView);

        const u32 drawCallCount            = indirectBuffer.writtenDrawCallBuffers[FIF].writtenDrawCount;
        const VkDeviceSize drawCallsSize   = sizeof(u32) + drawCallCount * sizeof(VkDrawIndexedIndirectCommand);
        const VkDeviceSize meshIndicesSize = drawCallCount * sizeof(u32);

        m_barrierWriter
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.opaqueBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .srcAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = drawCallsSize
            }
        )
        .WriteBufferBarrier(
            *indirectBuffer.frustumCulledBuffers.opaqueBuffer.meshIndexBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = meshIndicesSize
            }
        )
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.opaqueDoubleSidedBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .srcAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = drawCallsSize
            }
        )
        .WriteBufferBarrier(
            *indirectBuffer.frustumCulledBuffers.opaqueDoubleSidedBuffer.meshIndexBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = meshIndicesSize
            }
        )
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.alphaMaskedBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .srcAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = drawCallsSize
            }
        )
        .WriteBufferBarrier(
            *indirectBuffer.frustumCulledBuffers.alphaMaskedBuffer.meshIndexBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = meshIndicesSize
            }
        )
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .srcAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = drawCallsSize
            }
        )
        .WriteBufferBarrier(
            *indirectBuffer.frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.meshIndexBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = meshIndicesSize
            }
        )
        .Execute(cmdBuffer);
    }

    void Dispatch::Execute
    (
        usize FIF,
        const Vk::CommandBuffer& cmdBuffer,
        const Buffers::IndirectBuffer& indirectBuffer
    )
    {
        vkCmdDispatch
        (
            cmdBuffer.handle,
            GetWorkGroupCount(FIF, indirectBuffer),
            1,
            1
        );
    }

    void Dispatch::PostDispatch
    (
        usize FIF,
        const Vk::CommandBuffer& cmdBuffer,
        const Buffers::IndirectBuffer& indirectBuffer
    )
    {
        const u32 drawCallCount            = indirectBuffer.writtenDrawCallBuffers[FIF].writtenDrawCount;
        const VkDeviceSize drawCallsSize   = sizeof(u32) + drawCallCount * sizeof(VkDrawIndexedIndirectCommand);
        const VkDeviceSize meshIndicesSize = drawCallCount * sizeof(u32);

        m_barrierWriter
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.opaqueBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .dstAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = drawCallsSize
            }
        )
        .WriteBufferBarrier(
            *indirectBuffer.frustumCulledBuffers.opaqueBuffer.meshIndexBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = meshIndicesSize
            }
        )
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.opaqueDoubleSidedBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .dstAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = drawCallsSize
            }
        )
        .WriteBufferBarrier(
            *indirectBuffer.frustumCulledBuffers.opaqueDoubleSidedBuffer.meshIndexBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = meshIndicesSize
            }
        )
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.alphaMaskedBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .dstAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = drawCallsSize
            }
        )
        .WriteBufferBarrier(
            *indirectBuffer.frustumCulledBuffers.alphaMaskedBuffer.meshIndexBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = meshIndicesSize
            }
        )
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .dstAccessMask  = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = drawCallsSize
            }
        )
        .WriteBufferBarrier(
            *indirectBuffer.frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.meshIndexBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = meshIndicesSize
            }
        )
        .Execute(cmdBuffer);
    }

    u32 Dispatch::GetWorkGroupCount(usize FIF, const Buffers::IndirectBuffer& indirectBuffer)
    {
        return (indirectBuffer.writtenDrawCallBuffers[FIF].writtenDrawCount + CULLING_WORKGROUP_SIZE - 1) / CULLING_WORKGROUP_SIZE;
    }

    void Dispatch::Destroy(VmaAllocator allocator)
    {
        m_frustumBuffer.Destroy(allocator);
    }
}