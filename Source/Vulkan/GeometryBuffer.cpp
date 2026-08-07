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

#include "GeometryBuffer.h"

#include "DebugUtils.h"
#include "Models/Model.h"
#include "GPU/Vertex.h"
#include "Externals/ImGui.h"

namespace Vk
{
    GeometryBuffer::GeometryBuffer(const Vk::Context& context, Vk::StagingPool& stagingPool)
    {
        cubeBuffer = Vk::Buffer
        (
            context.device,
            context.allocator,
            36 * sizeof(GPU::Position),
            0,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        );

        SetupCubeUpload(context.device, context.allocator, stagingPool);

        Vk::SetDebugName(context.device, cubeBuffer.handle, "GeometryBuffer/CubeBuffer");
    }

    void GeometryBuffer::Bind(const Vk::CommandBuffer& cmdBuffer) const
    {
        indexBuffer.Bind(cmdBuffer);
    }

    void GeometryBuffer::Update
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::Context& context,
        Vk::StagingPool& stagingPool,
        Scratch::Allocator& scratchAllocator,
        Engine::DeletionQueue& deletionQueue
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        if (!HasPendingUploads())
        {
            return;
        }

        Vk::BeginLabel(cmdBuffer, "GeometryBuffer::Update", {0.9882f, 0.7294f, 0.0118f, 1.0f});

        indexBuffer.Update
        (
            cmdBuffer,
            context.device,
            context.allocator,
            scratchAllocator,
            deletionQueue
        );

        vertexBuffer.Update
        (
            cmdBuffer,
            context.device,
            context.allocator,
            scratchAllocator,
            deletionQueue
        );

        if (m_pendingCubeUpload.has_value())
        {
            Vk::BeginLabel(cmdBuffer, "Cube Transfer", {0.5117f, 0.0749f, 0.3901f, 1.0f});

            const VkBufferCopy2 copyRegion =
            {
                .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                .pNext     = nullptr,
                .srcOffset = m_pendingCubeUpload->memoryBlock.offset,
                .dstOffset = 0,
                .size      = m_pendingCubeUpload->memoryBlock.size
            };

            const VkCopyBufferInfo2 copyInfo =
            {
                .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
                .pNext       = nullptr,
                .srcBuffer   = m_pendingCubeUpload->buffer,
                .dstBuffer   = cubeBuffer.handle,
                .regionCount = 1,
                .pRegions    = &copyRegion
            };

            vkCmdCopyBuffer2(cmdBuffer.handle, &copyInfo);

            cubeBuffer.Barrier
            (
                cmdBuffer,
                Vk::BufferBarrier{
                    .srcStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                    .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .dstStageMask   = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                    .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                    .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .offset         = 0,
                    .size           = m_pendingCubeUpload->memoryBlock.size
                }
            );

            deletionQueue.Push([&stagingPool, stagingMemoryBlock = m_pendingCubeUpload.value()] () mutable
            {
                stagingPool.Free(stagingMemoryBlock);
            });

            m_pendingCubeUpload = std::nullopt;

            Vk::EndLabel(cmdBuffer);
        }

        Vk::EndLabel(cmdBuffer);

        Vk::SetDebugName(context.device, GetIndexBuffer().handle,    "GeometryBuffer/IndexBuffer");
        Vk::SetDebugName(context.device, GetPositionBuffer().handle, "GeometryBuffer/PositionBuffer");
        Vk::SetDebugName(context.device, GetUVBuffer().handle,       "GeometryBuffer/UVBuffer");
        Vk::SetDebugName(context.device, GetVertexBuffer().handle,   "GeometryBuffer/VertexBuffer");
    }

    void GeometryBuffer::Free(const GPU::SurfaceInfo& info, Engine::DeletionQueue& deletionQueue)
    {
        deletionQueue.Push([this, info] ()
        {
            indexBuffer.Free(info.indexInfo);
            vertexBuffer.Free(info.vertexInfo);
        });
    }

    void GeometryBuffer::SetupCubeUpload(VkDevice device, VmaAllocator allocator, Vk::StagingPool& stagingPool)
    {
        constexpr std::array CUBE_VERTICES =
        {
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f,  1.0f
        };

        constexpr VkDeviceSize VERTICES_SIZE = CUBE_VERTICES.size() * sizeof(f32);

        m_pendingCubeUpload = stagingPool.Allocate
        (
            device,
            allocator,
            VERTICES_SIZE,
            0
        );

        std::memcpy(m_pendingCubeUpload->hostAddress, CUBE_VERTICES.data(), VERTICES_SIZE);
    }

    void GeometryBuffer::ImGuiDisplay(Scratch::Allocator& scratchAllocator)
    {
        indexBuffer.ImGuiDisplay(scratchAllocator);
        vertexBuffer.ImGuiDisplay(scratchAllocator);
    }

    bool GeometryBuffer::HasPendingUploads()
    {
        return indexBuffer.HasPendingUploads() || vertexBuffer.HasPendingUploads() ||
               m_pendingCubeUpload.has_value();
    }

    const Vk::Buffer& GeometryBuffer::GetIndexBuffer() const
    {
        return indexBuffer.buffer;
    }

    const Vk::Buffer& GeometryBuffer::GetPositionBuffer() const
    {
        return vertexBuffer.positionBuffer;
    }

    const Vk::Buffer& GeometryBuffer::GetUVBuffer() const
    {
        return vertexBuffer.uvBuffer;
    }

    const Vk::Buffer& GeometryBuffer::GetVertexBuffer() const
    {
        return vertexBuffer.normalAndTangentBuffer;
    }

    void GeometryBuffer::Destroy(VmaAllocator allocator, Vk::StagingPool& stagingPool)
    {
        indexBuffer.Destroy(allocator);
        vertexBuffer.Destroy(allocator);
        cubeBuffer.Destroy(allocator);

        if (m_pendingCubeUpload.has_value())
        {
            stagingPool.Free(m_pendingCubeUpload.value());

            m_pendingCubeUpload = std::nullopt;
        }
    }
}