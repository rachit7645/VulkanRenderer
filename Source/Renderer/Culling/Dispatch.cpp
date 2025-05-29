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
#include "Util/Log.h"
#include "Culling/Frustum.h"

namespace Renderer::Culling
{
    constexpr auto CULLING_WORKGROUP_SIZE = 64;

    Dispatch::Dispatch(const Vk::Context& context)
        : m_frustumPipeline(context),
          m_frustumBuffer(context.device, context.allocator)
    {
    }

    void Dispatch::DispatchFrustumCulling
    (
        usize FIF,
        usize frameIndex,
        const glm::mat4& projectionView,
        const Vk::CommandBuffer& cmdBuffer,
        const Buffers::MeshBuffer& meshBuffer,
        const Buffers::IndirectBuffer& indirectBuffer
    )
    {
        Vk::BeginLabel(cmdBuffer, "FrustumCullingDispatch", glm::vec4(0.6196f, 0.5588f, 0.8588f, 1.0f));

        m_frustumBuffer.LoadPlanes(cmdBuffer, projectionView);

        m_frustumPipeline.Bind(cmdBuffer);

        const auto constants = Culling::Constants
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

        m_frustumPipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_COMPUTE_BIT,
            constants
        );

        Vk::BarrierWriter barrierWriter = {};

        const u32          drawCallCount   = indirectBuffer.writtenDrawCallBuffers[FIF].writtenDrawCount;
        const VkDeviceSize drawCallsSize   = sizeof(u32) + drawCallCount * sizeof(VkDrawIndexedIndirectCommand);
        const VkDeviceSize meshIndicesSize = drawCallCount * sizeof(u32);

        barrierWriter
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.opaqueBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask  = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .srcAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .dstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .offset        = 0,
                .size          = drawCallsSize
            }
        )
        .WriteBufferBarrier(
            *indirectBuffer.frustumCulledBuffers.opaqueBuffer.meshIndexBuffer,
            Vk::BufferBarrier{
                .srcStageMask  = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .dstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .offset        = 0,
                .size          = meshIndicesSize
            }
        )
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.opaqueDoubleSidedBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask  = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .srcAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .dstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .offset        = 0,
                .size          = drawCallsSize
            }
        )
        .WriteBufferBarrier(
            *indirectBuffer.frustumCulledBuffers.opaqueDoubleSidedBuffer.meshIndexBuffer,
            Vk::BufferBarrier{
                .srcStageMask  = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .dstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .offset        = 0,
                .size          = meshIndicesSize
            }
        )
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.alphaMaskedBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask  = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .srcAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .dstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .offset        = 0,
                .size          = drawCallsSize
            }
        )
        .WriteBufferBarrier(
            *indirectBuffer.frustumCulledBuffers.alphaMaskedBuffer.meshIndexBuffer,
            Vk::BufferBarrier{
                .srcStageMask  = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .dstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .offset        = 0,
                .size          = meshIndicesSize
            }
        )
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask  = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .srcAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .dstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .offset        = 0,
                .size          = drawCallsSize
            }
        )
        .WriteBufferBarrier(
            *indirectBuffer.frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.meshIndexBuffer,
            Vk::BufferBarrier{
                .srcStageMask  = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .dstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .offset        = 0,
                .size          = meshIndicesSize
            }
        )
        .Execute(cmdBuffer);

        vkCmdDispatch
        (
            cmdBuffer.handle,
            (drawCallCount + CULLING_WORKGROUP_SIZE - 1) / CULLING_WORKGROUP_SIZE,
            1,
            1
        );

        barrierWriter
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.opaqueBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask  = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .offset        = 0,
                .size          = drawCallsSize
            }
        )
        .WriteBufferBarrier(
            *indirectBuffer.frustumCulledBuffers.opaqueBuffer.meshIndexBuffer,
            Vk::BufferBarrier{
                .srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask  = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .offset        = 0,
                .size          = meshIndicesSize
            }
        )
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.opaqueDoubleSidedBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask  = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .offset        = 0,
                .size          = drawCallsSize
            }
        )
        .WriteBufferBarrier(
            *indirectBuffer.frustumCulledBuffers.opaqueDoubleSidedBuffer.meshIndexBuffer,
            Vk::BufferBarrier{
                .srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask  = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .offset        = 0,
                .size          = meshIndicesSize
            }
        )
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.alphaMaskedBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask  = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .offset        = 0,
                .size          = drawCallsSize
            }
        )
        .WriteBufferBarrier(
            *indirectBuffer.frustumCulledBuffers.alphaMaskedBuffer.meshIndexBuffer,
            Vk::BufferBarrier{
                .srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask  = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .offset        = 0,
                .size          = meshIndicesSize
            }
        )
        .WriteBufferBarrier(
            indirectBuffer.frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.drawCallBuffer,
            Vk::BufferBarrier{
                .srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask  = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                .dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                .offset        = 0,
                .size          = drawCallsSize
            }
        )
        .WriteBufferBarrier(
            *indirectBuffer.frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.meshIndexBuffer,
            Vk::BufferBarrier{
                .srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask  = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .offset        = 0,
                .size          = meshIndicesSize
            }
        )
        .Execute(cmdBuffer);

        Vk::EndLabel(cmdBuffer);
    }

    void Dispatch::Destroy(VkDevice device, VmaAllocator allocator)
    {
        m_frustumBuffer.Destroy(allocator);
        m_frustumPipeline.Destroy(device);
    }
}
