//    Copyright 2023 - 2024 Rachit Khandelwal
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.

#include "GeometryBuffer.h"

#include "Util.h"

namespace Vk
{
    constexpr VkDeviceSize INITIAL_BUFFER_SIZE = 64 * 1024 * 1024;
    constexpr f32          BUFFER_GROWTH_RATE  = 1.5f;

    GeometryBuffer::GeometryBuffer(VmaAllocator allocator)
    {
        vertexBuffer = Vk::Buffer
        (
            allocator,
            INITIAL_BUFFER_SIZE,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO
        );

        indexBuffer = Vk::Buffer
        (
            allocator,
            INITIAL_BUFFER_SIZE,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO
        );
    }

    void GeometryBuffer::Bind(const Vk::CommandBuffer& cmdBuffer) const
    {
        const std::array<VkBuffer, 1>     buffers = {vertexBuffer.handle};
        const std::array<VkDeviceSize, 1> offsets = {0};

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

        vkCmdBindIndexBuffer(cmdBuffer.handle, indexBuffer.handle, 0, indexType);
    }

    std::array<Models::VertexInfo, 2> GeometryBuffer::LoadData
    (
        const Vk::Context& context,
        const std::span<const Models::Vertex> vertices,
        const std::span<const Models::Index> indices
    )
    {
        std::array<Models::VertexInfo, 2> infos;

        Vk::ImmediateSubmit(context, [&](const Vk::CommandBuffer& cmdBuffer)
        {
            infos[0] = UploadData(cmdBuffer, context.allocator, vertexBuffer, vertexCount, vertices);
            infos[1] = UploadData(cmdBuffer, context.allocator, indexBuffer,  indexCount,  indices );
        });

        return infos;
    }

    template<typename T>
    Models::VertexInfo GeometryBuffer::UploadData
    (
        const Vk::CommandBuffer& cmdBuffer,
        VmaAllocator allocator,
        const Vk::Buffer& buffer,
        u32& currentCount,
        const std::span<const T> data
    )
    {
        // Bytes
        const VkDeviceSize size   = data.size()  * sizeof(T);
        const VkDeviceSize offset = currentCount * sizeof(T);

        auto stagingBuffer = Vk::Buffer
        (
            allocator,
            size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            VMA_MEMORY_USAGE_AUTO
        );

        stagingBuffer.LoadData(allocator, data);

        const VkBufferCopy2 copyRegion =
        {
            .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
            .pNext     = nullptr,
            .srcOffset = 0,
            .dstOffset = offset,
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

        stagingBuffer.Destroy(allocator);

        auto vertexInfo = Models::VertexInfo(currentCount, data.size());
        currentCount   += data.size();

        return vertexInfo;
    }

    void GeometryBuffer::Destroy(VmaAllocator allocator) const
    {
        // Delete buffers
        vertexBuffer.Destroy(allocator);
        indexBuffer.Destroy(allocator);
    }
}
