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

#include "ResizableAllocator.h"
#include "Util/Log.h"
#include "Util/Scope.h"
#include "Vulkan/BarrierWriter.h"
#include "Vulkan/DebugUtils.h"

namespace Vk
{
    ResizableAllocator::ResizableAllocator
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

    Vk::MemoryBlock ResizableAllocator::Allocate(VkDeviceSize size)
    {
        if (size == 0)
        {
            Logger::Error("{}\n", "Can't allocate a block of size zero!");
        }

        if (const auto block = FindFreeBlock(size); block.has_value())
        {
            return block.value();
        }

        const auto lastUsedBlock = m_usedBlocks.empty() ? Vk::MemoryBlock{} : *m_usedBlocks.rbegin();
        const auto lastFreeBlock = m_freeBlocks.empty() ? Vk::MemoryBlock{} : *m_freeBlocks.rbegin();

        const auto lastBlock = std::max(lastUsedBlock, lastFreeBlock);

        const Vk::MemoryBlock block =
        {
            .offset = lastBlock.offset + lastBlock.size,
            .size   = size,
        };

        const VkDeviceSize minRequiredCapacity = block.offset + block.size;

        if (minRequiredCapacity > m_capacity)
        {
            QueueResize(minRequiredCapacity);
        }

        m_usedBlocks.insert(block);

        return block;
    }

    void ResizableAllocator::Free(const Vk::MemoryBlock& block)
    {
        if (m_freeBlocks.contains(block))
        {
            Logger::Error("Block already freed! [Offset={}] [Size={}]\n", block.offset, block.size);
        }

        if (!m_usedBlocks.contains(block))
        {
            Logger::Error("Invalid block! [Offset={}] [Size={}]\n", block.offset, block.size);
        }

        m_usedBlocks.erase(block);
        m_freeBlocks.insert(block);

        MergeFreeBlocks();
    }

    void ResizableAllocator::Update
    (
        const Vk::CommandBuffer& cmdBuffer,
        VkDevice device,
        VmaAllocator allocator,
        Util::DeletionQueue& deletionQueue
    )
    {
        auto Reset = Util::MakeScopeGuard([this] ()
        {
            m_resizeCopyBlocks = std::nullopt;
        });

        // Capacity Check

        if (m_capacity == 0 || m_oldCapacity == m_capacity)
        {
            return;
        }

        m_oldCapacity = m_capacity;

        // Buffer Resize

        auto oldBuffer = buffer;

        deletionQueue.Push([allocator, buffer = oldBuffer] () mutable
        {
            buffer.Destroy(allocator);
        });

        buffer = Vk::Buffer
        (
            device,
            allocator,
            m_capacity,
            0,
            m_usage,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        );

        // Copy old elements

        if (oldBuffer.handle == VK_NULL_HANDLE || !m_resizeCopyBlocks.has_value())
        {
            return;
        }

        if (m_resizeCopyBlocks->empty())
        {
            return;
        }

        if (m_usedBlocks.empty())
        {
            return;
        }

        std::vector<VkBufferCopy2> copyRegions = {};

        Vk::BarrierWriter barrierWriterOld = {};
        Vk::BarrierWriter barrierWriterNew = {};

        for (const auto& block : *m_resizeCopyBlocks)
        {
            if (!m_usedBlocks.contains(block))
            {
                continue;
            }

            copyRegions.emplace_back(VkBufferCopy2{
                .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                .pNext     = nullptr,
                .srcOffset = block.offset,
                .dstOffset = block.offset,
                .size      = block.size
            });

            barrierWriterOld.WriteBufferBarrier
            (
                oldBuffer,
                Vk::BufferBarrier{
                    .srcStageMask   = m_stageMask,
                    .srcAccessMask  = m_accessMask,
                    .dstStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                    .dstAccessMask  = VK_ACCESS_2_TRANSFER_READ_BIT,
                    .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .offset         = block.offset,
                    .size           = block.size
                }
            );

            barrierWriterNew.WriteBufferBarrier
            (
                buffer,
                Vk::BufferBarrier{
                    .srcStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                    .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .dstStageMask   = m_stageMask,
                    .dstAccessMask  = m_accessMask,
                    .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .offset         = block.offset,
                    .size           = block.size
                }
            );
        }

        if (copyRegions.empty())
        {
            return;
        }

        Vk::BeginLabel(cmdBuffer, "Block Allocator -> Resize Copy", {0.3882f, 0.9294f, 0.2118f, 1.0f});

        const VkCopyBufferInfo2 copyInfo =
        {
            .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
            .pNext       = nullptr,
            .srcBuffer   = oldBuffer.handle,
            .dstBuffer   = buffer.handle,
            .regionCount = static_cast<u32>(copyRegions.size()),
            .pRegions    = copyRegions.data(),
        };

        barrierWriterOld.Execute(cmdBuffer);

        vkCmdCopyBuffer2(cmdBuffer.handle, &copyInfo);

        barrierWriterNew.Execute(cmdBuffer);

        Vk::EndLabel(cmdBuffer);
    }

    void ResizableAllocator::QueueResize(VkDeviceSize minRequiredCapacity)
    {
        constexpr f64 BUFFER_GROWTH_FACTOR = 1.2;

        m_capacity = static_cast<VkDeviceSize>(BUFFER_GROWTH_FACTOR * static_cast<f64>(minRequiredCapacity));

        if (!m_resizeCopyBlocks.has_value())
        {
            m_resizeCopyBlocks = m_usedBlocks;
        }
    }

    std::optional<Vk::MemoryBlock> ResizableAllocator::FindFreeBlock(VkDeviceSize size)
    {
        for (const auto& block : m_freeBlocks)
        {
            if (block.size < size)
            {
                continue;
            }

            if (block.size == size)
            {
                const auto copy = block;

                m_usedBlocks.insert(block);
                m_freeBlocks.erase(block);

                return copy;
            }

            if (block.size > size)
            {
                const Vk::MemoryBlock free =
                {
                    .offset = block.offset,
                    .size   = size
                };

                const Vk::MemoryBlock remaining =
                {
                    .offset = block.offset + free.size,
                    .size   = block.size   - free.size
                };

                m_usedBlocks.insert(free);
                m_freeBlocks.erase(block);
                m_freeBlocks.insert(remaining);

                return free;
            }
        }

        return std::nullopt;
    }

    void ResizableAllocator::MergeFreeBlocks()
    {
        if (m_freeBlocks.size() <= 1)
        {
            return;
        }

        std::set<Vk::MemoryBlock> mergedBlocks;

        auto it = m_freeBlocks.begin();
        Vk::MemoryBlock currentBlock = *it;
        ++it;

        while (it != m_freeBlocks.end())
        {
            const auto& nextBlock = *it;

            if (currentBlock.offset + currentBlock.size == nextBlock.offset)
            {
                currentBlock.size += nextBlock.size;
            }
            else
            {
                mergedBlocks.insert(currentBlock);
                currentBlock = nextBlock;
            }

            ++it;
        }

        mergedBlocks.insert(currentBlock);

        m_freeBlocks = std::move(mergedBlocks);
    }

    void ResizableAllocator::Destroy(VmaAllocator allocator)
    {
        buffer.Destroy(allocator);
    }
}
