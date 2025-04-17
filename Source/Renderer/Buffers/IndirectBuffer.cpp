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

#include "IndirectBuffer.h"

#include "MeshBuffer.h"
#include "Util/Log.h"
#include "Vulkan/DebugUtils.h"
#include "Vulkan/ImmediateSubmit.h"

namespace Renderer::Buffers
{
    IndirectBuffer::IndirectBuffer(const Vk::Context& context, Vk::CommandBufferAllocator& cmdBufferAllocator)
    {
        for (usize i = 0; i < drawCallBuffers.size(); ++i)
        {
            drawCallBuffers[i] = DrawCallBuffer(context.device, context.allocator, DrawCallBuffer::Type::CPUToGPU);

            const u32 zero = 0;

            std::memcpy(drawCallBuffers[i].drawCallBuffer.allocationInfo.pMappedData, &zero, sizeof(u32));

            if (!(drawCallBuffers[i].drawCallBuffer.memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
            {
                Vk::CheckResult(vmaFlushAllocation(
                    context.allocator,
                    drawCallBuffers[i].drawCallBuffer.allocation,
                    0,
                    sizeof(u32)),
                    "Failed to flush allocation!"
                );
            }

            Vk::SetDebugName(context.device, drawCallBuffers[i].drawCallBuffer.handle,  fmt::format("IndirectBuffer/DrawCallBuffer/DrawCalls/{}",   i));
            Vk::SetDebugName(context.device, drawCallBuffers[i].meshIndexBuffer.handle, fmt::format("IndirectBuffer/DrawCallBuffer/MeshIndices/{}", i));
        }

        frustumCulledDrawCallBuffer = DrawCallBuffer(context.device, context.allocator, DrawCallBuffer::Type::GPUOnly);

        occlusionCulledPreviouslyVisibleDrawCallBuffer = DrawCallBuffer(context.device, context.allocator, DrawCallBuffer::Type::GPUOnly);
        occlusionCulledNewlyVisibleDrawCallBuffer      = DrawCallBuffer(context.device, context.allocator, DrawCallBuffer::Type::GPUOnly);
        occlusionCulledTotalVisibleDrawCallBuffer      = DrawCallBuffer(context.device, context.allocator, DrawCallBuffer::Type::GPUOnly);

        visibilityBuffer = Vk::Buffer
        (
            context.allocator,
            sizeof(u32) * MAX_MESH_COUNT,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        );

        visibilityBuffer.GetDeviceAddress(context.device);

        Vk::ImmediateSubmit
        (
            context.device,
            context.graphicsQueue,
            cmdBufferAllocator,
            [&] (const Vk::CommandBuffer& cmdBuffer)
            {
                // Zero out initial draw counts

                vkCmdFillBuffer
                (
                    cmdBuffer.handle,
                    frustumCulledDrawCallBuffer.drawCallBuffer.handle,
                    0,
                    sizeof(u32),
                    0
                );

                frustumCulledDrawCallBuffer.drawCallBuffer.Barrier
                (
                    cmdBuffer,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                    0,
                    sizeof(u32)
                );

                vkCmdFillBuffer
                (
                    cmdBuffer.handle,
                    occlusionCulledPreviouslyVisibleDrawCallBuffer.drawCallBuffer.handle,
                    0,
                    sizeof(u32),
                    0
                );

                occlusionCulledPreviouslyVisibleDrawCallBuffer.drawCallBuffer.Barrier
                (
                    cmdBuffer,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
                    VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
                    0,
                    sizeof(u32)
                );

                vkCmdFillBuffer
                (
                    cmdBuffer.handle,
                    occlusionCulledNewlyVisibleDrawCallBuffer.drawCallBuffer.handle,
                    0,
                    sizeof(u32),
                    0
                );

                occlusionCulledNewlyVisibleDrawCallBuffer.drawCallBuffer.Barrier
                (
                    cmdBuffer,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                    0,
                    sizeof(u32)
                );

                vkCmdFillBuffer
                (
                    cmdBuffer.handle,
                    occlusionCulledTotalVisibleDrawCallBuffer.drawCallBuffer.handle,
                    0,
                    sizeof(u32),
                    0
                );

                occlusionCulledTotalVisibleDrawCallBuffer.drawCallBuffer.Barrier
                (
                    cmdBuffer,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                    0,
                    sizeof(u32)
                );

                // Mark all draws as not previously visible

                vkCmdFillBuffer
                (
                    cmdBuffer.handle,
                    visibilityBuffer.handle,
                    0,
                    VK_WHOLE_SIZE,
                    0
                );

                occlusionCulledTotalVisibleDrawCallBuffer.drawCallBuffer.Barrier
                (
                    cmdBuffer,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                    0,
                    VK_WHOLE_SIZE
                );
            }
        );

        Vk::SetDebugName(context.device, frustumCulledDrawCallBuffer.drawCallBuffer.handle,  "IndirectBuffer/DrawCallBuffer/FrustumCulled/DrawCalls");
        Vk::SetDebugName(context.device, frustumCulledDrawCallBuffer.meshIndexBuffer.handle, "IndirectBuffer/DrawCallBuffer/FrustumCulled/MeshIndices");

        Vk::SetDebugName(context.device, occlusionCulledPreviouslyVisibleDrawCallBuffer.drawCallBuffer.handle,  "IndirectBuffer/DrawCallBuffer/OcclusionCulled/PreviouslyVisibleOrTotalVisible/0/DrawCalls");
        Vk::SetDebugName(context.device, occlusionCulledPreviouslyVisibleDrawCallBuffer.meshIndexBuffer.handle, "IndirectBuffer/DrawCallBuffer/OcclusionCulled/PreviouslyVisibleOrTotalVisible/0/MeshIndices");

        Vk::SetDebugName(context.device, occlusionCulledNewlyVisibleDrawCallBuffer.drawCallBuffer.handle,  "IndirectBuffer/DrawCallBuffer/OcclusionCulled/NewlyVisible/DrawCalls");
        Vk::SetDebugName(context.device, occlusionCulledNewlyVisibleDrawCallBuffer.meshIndexBuffer.handle, "IndirectBuffer/DrawCallBuffer/OcclusionCulled/NewlyVisible/MeshIndices");

        Vk::SetDebugName(context.device, occlusionCulledTotalVisibleDrawCallBuffer.drawCallBuffer.handle,  "IndirectBuffer/DrawCallBuffer/OcclusionCulled/PreviouslyVisibleOrTotalVisible/1/DrawCalls");
        Vk::SetDebugName(context.device, occlusionCulledTotalVisibleDrawCallBuffer.meshIndexBuffer.handle, "IndirectBuffer/DrawCallBuffer/OcclusionCulled/PreviouslyVisibleOrTotalVisible/1/MeshIndices");

        Vk::SetDebugName(context.device, visibilityBuffer.handle, "IndirectBuffer/MeshVisibilityLUT");
    }

    void IndirectBuffer::WriteDrawCalls
    (
        usize FIF,
        VmaAllocator allocator,
        const Models::ModelManager& modelManager,
        const std::span<const Renderer::RenderObject> renderObjects
    )
    {
        drawCallBuffers[FIF].WriteDrawCalls(allocator, modelManager, renderObjects);
    }

    void IndirectBuffer::Destroy(VmaAllocator allocator)
    {
        for (auto& buffer : drawCallBuffers)
        {
            buffer.Destroy(allocator);
        }

        frustumCulledDrawCallBuffer.Destroy(allocator);

        occlusionCulledPreviouslyVisibleDrawCallBuffer.Destroy(allocator);
        occlusionCulledNewlyVisibleDrawCallBuffer.Destroy(allocator);
        occlusionCulledTotalVisibleDrawCallBuffer.Destroy(allocator);

        visibilityBuffer.Destroy(allocator);
    }
}