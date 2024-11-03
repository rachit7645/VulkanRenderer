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

#include <Util/Log.h>

#include "Util.h"

namespace Vk
{
    // TODO: Implement resizing

    constexpr VkDeviceSize INITIAL_BUFFER_SIZE = 64 * 1024 * 1024;
    constexpr VkDeviceSize STAGING_BUFFER_SIZE = 16 * 1024 * 1024;
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

        m_stagingBuffer = Vk::Buffer
        (
            allocator,
            STAGING_BUFFER_SIZE,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
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
            Logger::Warning("Current Vertex/Index Count: {}/{}\n", vertexCount, indexCount);

            infos[0] = UploadData(cmdBuffer, context.allocator, vertexBuffer, vertexCount, vertices);
            vertexCount += infos[0].count;

            infos[1] = UploadData(cmdBuffer, context.allocator, indexBuffer, indexCount, indices);
            indexCount += infos[1].count;
        });

        return infos;
    }

    template<typename T>
    Models::VertexInfo GeometryBuffer::UploadData
    (
        const Vk::CommandBuffer& cmdBuffer,
        VmaAllocator allocator,
        const Vk::Buffer& buffer,
        u32 currentCount,
        const std::span<const T> data
    )
    {
        // Bytes
        const VkDeviceSize size   = data.size()  * sizeof(T);
        const VkDeviceSize offset = currentCount * sizeof(T);

        m_stagingBuffer.Map(allocator);
            std::memcpy(m_stagingBuffer.allocInfo.pMappedData, data.data(), size);
        m_stagingBuffer.Unmap(allocator);

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
            .srcBuffer   = m_stagingBuffer.handle,
            .dstBuffer   = buffer.handle,
            .regionCount = 1,
            .pRegions    = &copyRegion
        };

        vkCmdCopyBuffer2(cmdBuffer.handle, &copyInfo);

        VkBufferMemoryBarrier2 barrier =
        {
            .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .pNext               = nullptr,
            .srcStageMask        = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask        = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT,
            .dstAccessMask       = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_2_INDEX_READ_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer              = buffer.handle,
            .offset              = offset,
            .size                = size
        };

        VkDependencyInfo dependencyInfo =
        {
            .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext                    = nullptr,
            .dependencyFlags          = 0,
            .memoryBarrierCount       = 0,
            .pMemoryBarriers          = nullptr,
            .bufferMemoryBarrierCount = 0,
            .pBufferMemoryBarriers    = &barrier,
            .imageMemoryBarrierCount  = 0,
            .pImageMemoryBarriers     = nullptr
        };

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencyInfo);

        return
        {
            .offset = static_cast<u32>(offset / sizeof(T)),
            .count  = static_cast<u32>(size   / sizeof(T))
        };
    }

    void GeometryBuffer::Destroy(VmaAllocator allocator) const
    {
        // Delete buffers
        vertexBuffer.Destroy(allocator);
        indexBuffer.Destroy(allocator);
        m_stagingBuffer.Destroy(allocator);
    }
}
