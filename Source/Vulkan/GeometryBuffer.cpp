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

#include <thread>
#include <Util/Log.h>

#include "Util.h"

namespace Vk
{
    // TODO: Implement resizing

    constexpr VkDeviceSize INITIAL_BUFFER_SIZE = 64 * 1024 * 1024;
    constexpr VkDeviceSize STAGING_BUFFER_SIZE = 16 * 1024 * 1024;
    constexpr f32          BUFFER_GROWTH_RATE  = 1.5f;

    GeometryBuffer::GeometryBuffer(VkDevice device, VmaAllocator allocator)
    {
        vertexBuffer = Vk::Buffer
        (
            allocator,
            INITIAL_BUFFER_SIZE,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            VMA_MEMORY_USAGE_AUTO
        );

        vertexBuffer.GetDeviceAddress(device);

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
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            VMA_MEMORY_USAGE_AUTO
        );
    }

    void GeometryBuffer::Bind(const Vk::CommandBuffer& cmdBuffer) const
    {
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
            infos[0] = UploadData(cmdBuffer, vertexBuffer, vertexCount, vertices);
            infos[1] = UploadData(cmdBuffer, indexBuffer,  indexCount,  indices);
        });

        return infos;
    }

    template<typename T>
    Models::VertexInfo GeometryBuffer::UploadData
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::Buffer& buffer,
        u32& currentCount,
        const std::span<const T> data
    )
    {
        // Bytes
        const VkDeviceSize size   = data.size()  * sizeof(T);
        const VkDeviceSize offset = currentCount * sizeof(T);

        if (size > m_stagingBuffer.allocInfo.size)
        {
            Logger::Error
            (
                "Not enough space in staging buffer! [Required={}] [Available={}]\n",
                size,
                m_stagingBuffer.allocInfo.size
            );
        }

        if ((size + offset) > buffer.allocInfo.size)
        {
            Logger::Error
            (
                "Not enough space in buffer! [Required={}] [Available={}]\n",
                size,
                buffer.allocInfo.size
            );
        }

        std::memcpy(m_stagingBuffer.allocInfo.pMappedData, data.data(), size);

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

        const VkBufferMemoryBarrier2 barrier =
        {
            .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .pNext               = nullptr,
            .srcStageMask        = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask        = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
            .dstAccessMask       = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_INDEX_READ_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer              = buffer.handle,
            .offset              = offset,
            .size                = size
        };

        const VkDependencyInfo dependencyInfo =
        {
            .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext                    = nullptr,
            .dependencyFlags          = 0,
            .memoryBarrierCount       = 0,
            .pMemoryBarriers          = nullptr,
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers    = &barrier,
            .imageMemoryBarrierCount  = 0,
            .pImageMemoryBarriers     = nullptr
        };

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencyInfo);

        Models::VertexInfo info =
        {
            .offset = currentCount,
            .count  = static_cast<u32>(data.size())
        };

        currentCount += info.count;

        return info;
    }

    void GeometryBuffer::Destroy(VmaAllocator allocator) const
    {
        // Delete buffers
        vertexBuffer.Destroy(allocator);
        indexBuffer.Destroy(allocator);
        m_stagingBuffer.Destroy(allocator);
    }
}
