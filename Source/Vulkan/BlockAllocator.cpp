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

#include "BlockAllocator.h"
#include "Util/Log.h"

namespace Vk
{
    bool BlockAllocator::Block::operator==(const Block& other) const noexcept
    {
        return offset == other.offset && size == other.size;
    }

    bool BlockAllocator::Block::operator<(const Block& other) const noexcept
    {
        return offset < other.offset;
    }

    BlockAllocator::BlockAllocator
    (
        VkBufferUsageFlags usage,
        VkPipelineStageFlags2 stageMask,
        VkAccessFlags2 accessMask
    )
        : m_usage(usage),
          m_stageMask(stageMask),
          m_accessMask(accessMask)
    {
    }

    BlockAllocator::Block BlockAllocator::Allocate(VkDeviceSize size)
    {
        const auto lastUsedBlock = m_usedBlocks.empty() ? BlockAllocator::Block{} : *m_usedBlocks.rbegin();

        const auto block = BlockAllocator::Block
        {
            .offset = lastUsedBlock.offset + lastUsedBlock.size,
            .size   = size,
        };

        if (m_usedBlocks.contains(block))
        {
            Logger::Error("Block already exists! [Offset={}] [Size={}]\n", block.offset, block.size);
        }

        if (block.offset + block.size > m_capacity)
        {
            QueueResize(block.offset, block.offset + block.size);
        }

        m_usedBlocks.insert(block);

        return block;
    }

    void BlockAllocator::Update
    (
        const Vk::CommandBuffer& cmdBuffer,
        VkDevice device,
        VmaAllocator allocator,
        Util::DeletionQueue& deletionQueue
    )
    {
        if (m_capacity == 0 || m_oldCapacity == m_capacity)
        {
            return;
        }

        auto oldBuffer = buffer;

        deletionQueue.PushDeletor([allocator, buffer = oldBuffer] () mutable
        {
            buffer.Destroy(allocator);
        });

        buffer = Vk::Buffer
        (
            allocator,
            m_capacity,
            m_usage,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        );

        if (m_usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
        {
            buffer.GetDeviceAddress(device);
        }

        if (!m_resizeCopyOffset.has_value() || oldBuffer.handle == VK_NULL_HANDLE)
        {
            return;
        }

        oldBuffer.Barrier
        (
            cmdBuffer,
            Vk::BufferBarrier{
                .srcStageMask  = m_stageMask,
                .srcAccessMask = m_accessMask,
                .dstStageMask  = VK_PIPELINE_STAGE_2_COPY_BIT,
                .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
                .offset        = 0,
                .size          = *m_resizeCopyOffset
            }
        );

        const VkBufferCopy2 copyRegion =
        {
            .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
            .pNext     = nullptr,
            .srcOffset = 0,
            .dstOffset = 0,
            .size      = *m_resizeCopyOffset
        };

        const VkCopyBufferInfo2 copyInfo =
        {
            .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
            .pNext       = nullptr,
            .srcBuffer   = oldBuffer.handle,
            .dstBuffer   = buffer.handle,
            .regionCount = 1,
            .pRegions    = &copyRegion,
        };

        vkCmdCopyBuffer2(cmdBuffer.handle, &copyInfo);

        m_resizeCopyOffset = std::nullopt;
        m_oldCapacity      = m_capacity;
    }

    void BlockAllocator::QueueResize(VkDeviceSize currentMaxOffset, VkDeviceSize minRequiredCapacity)
    {
        constexpr f64 BUFFER_GROWTH_FACTOR = 1.5f;

        m_capacity = static_cast<VkDeviceSize>(BUFFER_GROWTH_FACTOR * static_cast<f64>(minRequiredCapacity));

        if (!m_resizeCopyOffset.has_value())
        {
            m_resizeCopyOffset = currentMaxOffset;
        }
    }

    void BlockAllocator::Destroy(VmaAllocator allocator)
    {
        buffer.Destroy(allocator);
    }
}
