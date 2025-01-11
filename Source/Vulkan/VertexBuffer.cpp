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

#include "VertexBuffer.h"

#include "Util.h"
#include "Util/Log.h"

// Usings
using Models::Vertex;
using Models::Index;

namespace Vk
{
    VertexBuffer::VertexBuffer
    (
        const Vk::Context& context,
        const std::span<const Vertex> vertices,
        const std::span<const Index> indices
    )
        : vertexCount(static_cast<u32>(vertices.size())),
          indexCount(static_cast<u32>(indices.size()))
    {
        vertexBuffer = InitBuffer(context, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertices);
        indexBuffer  = InitBuffer(context, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,  indices);
    }

    VertexBuffer::VertexBuffer(const Vk::Context& context, const std::span<const f32> vertices)
        : vertexCount(vertices.size())
    {
        // No index buffer
        vertexBuffer = InitBuffer(context, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertices);
    }

    void VertexBuffer::Bind(const Vk::CommandBuffer& cmdBuffer) const
    {
        std::array<VkBuffer, 1>     buffers = {vertexBuffer.handle};
        std::array<VkDeviceSize, 1> offsets = {0};

        vkCmdBindVertexBuffers2
        (
            cmdBuffer.handle,
            0,
            static_cast<u32>(buffers.size()),
            buffers.data(),
            offsets.data(),
            nullptr,
            nullptr
        );

        if (indexBuffer.has_value())
        {
            vkCmdBindIndexBuffer(cmdBuffer.handle, indexBuffer->handle, 0, indexType);
        }
    }

    template <typename T>
    Vk::Buffer VertexBuffer::InitBuffer
    (
        const Vk::Context& context,
        VkBufferUsageFlags usage,
        const std::span<const T> data
    )
    {
        // Bytes
        VkDeviceSize size = data.size() * sizeof(T);

        auto stagingBuffer = Vk::Buffer
        (
            context.allocator,
            size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            VMA_MEMORY_USAGE_AUTO
        );

        std::memcpy(stagingBuffer.allocInfo.pMappedData, data.data(), size);

        auto buffer = Vk::Buffer
        (
            context.allocator,
            size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO
        );

        Vk::ImmediateSubmit(context, [&](const Vk::CommandBuffer& cmdBuffer)
        {
            const VkBufferCopy2 copyRegion =
            {
                .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                .pNext     = nullptr,
                .srcOffset = 0,
                .dstOffset = 0,
                .size      = size
            };

            const VkCopyBufferInfo2 copyInfo =
            {
                .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
                .pNext       = nullptr,
                .srcBuffer   = stagingBuffer.handle,
                .dstBuffer   = buffer.handle,
                .regionCount = 1,
                .pRegions    = &copyRegion
            };

            vkCmdCopyBuffer2(cmdBuffer.handle, &copyInfo);
        });

        stagingBuffer.Destroy(context.allocator);

        return buffer;
    }

    void VertexBuffer::Destroy(VmaAllocator allocator) const
    {
        vertexBuffer.Destroy(allocator);

        if (indexBuffer.has_value())
        {
            indexBuffer->Destroy(allocator);
        }
    }
}