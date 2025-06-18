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

#include "BarrierWriter.h"

namespace Vk
{
    BarrierWriter& BarrierWriter::WriteBufferBarrier(const Vk::Buffer& buffer, const Vk::BufferBarrier& barrier)
    {
        m_bufferBarriers.emplace_back(VkBufferMemoryBarrier2{
            .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .pNext               = nullptr,
            .srcStageMask        = barrier.srcStageMask,
            .srcAccessMask       = barrier.srcAccessMask,
            .dstStageMask        = barrier.dstStageMask,
            .dstAccessMask       = barrier.dstAccessMask,
            .srcQueueFamilyIndex = barrier.srcQueueFamily,
            .dstQueueFamilyIndex = barrier.dstQueueFamily,
            .buffer              = buffer.handle,
            .offset              = barrier.offset,
            .size                = barrier.size
        });

        return *this;
    }

    BarrierWriter& BarrierWriter::WriteImageBarrier(const Vk::Image& image, const ImageBarrier& barrier)
    {
        m_imageBarriers.emplace_back(VkImageMemoryBarrier2{
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext               = nullptr,
            .srcStageMask        = barrier.srcStageMask,
            .srcAccessMask       = barrier.srcAccessMask,
            .dstStageMask        = barrier.dstStageMask,
            .dstAccessMask       = barrier.dstAccessMask,
            .oldLayout           = barrier.oldLayout,
            .newLayout           = barrier.newLayout,
            .srcQueueFamilyIndex = barrier.srcQueueFamily,
            .dstQueueFamilyIndex = barrier.dstQueueFamily,
            .image               = image.handle,
            .subresourceRange    = {
                .aspectMask     = image.aspect,
                .baseMipLevel   = barrier.baseMipLevel,
                .levelCount     = barrier.levelCount,
                .baseArrayLayer = barrier.baseArrayLayer,
                .layerCount     = barrier.layerCount,
            }
        });

        return *this;
    }

    void BarrierWriter::Execute(const Vk::CommandBuffer& cmdBuffer)
    {
        if (IsEmpty())
        {
            return;
        }

        const VkDependencyInfo dependencyInfo =
        {
            .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext                    = nullptr,
            .dependencyFlags          = 0,
            .memoryBarrierCount       = 0,
            .pMemoryBarriers          = nullptr,
            .bufferMemoryBarrierCount = static_cast<u32>(m_bufferBarriers.size()),
            .pBufferMemoryBarriers    = m_bufferBarriers.data(),
            .imageMemoryBarrierCount  = static_cast<u32>(m_imageBarriers.size()),
            .pImageMemoryBarriers     = m_imageBarriers.data()
        };

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencyInfo);

        Clear();
    }

    BarrierWriter& BarrierWriter::Clear()
    {
        m_bufferBarriers.clear();
        m_imageBarriers.clear();

        return *this;
    }

    bool BarrierWriter::IsEmpty() const
    {
        return m_bufferBarriers.empty() && m_imageBarriers.empty();
    }
}
