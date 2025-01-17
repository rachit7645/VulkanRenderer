// Copyright (c) 2023 - 2025 Rachit
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "GeometryBuffer.h"

#include <Models/Model.h>

#include "Util/Log.h"

namespace Vk
{
    // TODO: Implement resizing
    constexpr VkDeviceSize INITIAL_INDEX_SIZE = (1 << 24) * sizeof(Models::Index);
    constexpr VkDeviceSize STAGING_BUFFER_SIZE = 16 * 1024 * 1024;

    GeometryBuffer::GeometryBuffer(VmaAllocator allocator)
    {
        indexBuffer = Vk::Buffer
        (
            allocator,
            INITIAL_INDEX_SIZE,
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
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            VMA_MEMORY_USAGE_AUTO
        );
    }

    void GeometryBuffer::Bind(const Vk::CommandBuffer& cmdBuffer) const
    {
        vkCmdBindIndexBuffer(cmdBuffer.handle, indexBuffer.handle, 0, indexType);
    }

    Models::Info GeometryBuffer::LoadIndices(const Vk::Context& context, const std::span<const Models::Index> indices)
    {
        Models::Info indexInfo = {};

        Vk::ImmediateSubmit(context, [&](const Vk::CommandBuffer& cmdBuffer)
        {
            indexInfo = UploadIndices(cmdBuffer, indices);
        });

        return indexInfo;
    }

    Models::Info GeometryBuffer::UploadIndices(const Vk::CommandBuffer& cmdBuffer, const std::span<const Models::Index> data)
    {
        // Bytes
        const VkDeviceSize sizeBytes   = data.size() * sizeof(Models::Index);
        const VkDeviceSize offsetBytes = indexCount  * sizeof(Models::Index);

        if (sizeBytes > m_stagingBuffer.allocInfo.size)
        {
            Logger::Error
            (
                "Not enough space in staging buffer! [Required={}] [Available={}]\n",
                sizeBytes,
                m_stagingBuffer.allocInfo.size
            );
        }

        if ((sizeBytes + offsetBytes) > indexBuffer.allocInfo.size)
        {
            Logger::Error
            (
                "Not enough space in buffer! [Required={}] [Available={}]\n",
                sizeBytes,
                indexBuffer.allocInfo.size
            );
        }

        std::memcpy(m_stagingBuffer.allocInfo.pMappedData, data.data(), sizeBytes);

        const VkBufferCopy2 copyRegion =
        {
            .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
            .pNext     = nullptr,
            .srcOffset = 0,
            .dstOffset = offsetBytes,
            .size      = sizeBytes
        };

        const VkCopyBufferInfo2 copyInfo =
        {
            .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
            .pNext       = nullptr,
            .srcBuffer   = m_stagingBuffer.handle,
            .dstBuffer   = indexBuffer.handle,
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
            .dstStageMask        = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
            .dstAccessMask       = VK_ACCESS_2_INDEX_READ_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer              = indexBuffer.handle,
            .offset              = offsetBytes,
            .size                = sizeBytes
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

        const Models::Info info =
        {
            .offset = indexCount,
            .count  = static_cast<u32>(data.size())
        };

        indexCount += info.count;

        return info;
    }

    void GeometryBuffer::Destroy(VmaAllocator allocator) const
    {
        indexBuffer.Destroy(allocator);
        m_stagingBuffer.Destroy(allocator);
    }
}