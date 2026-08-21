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

#include "StagingPool.h"

#include "Util.h"
#include "Util/Memory.h"
#include "Util/Log.h"
#include "Vulkan/DebugUtils.h"

namespace Vk
{
    void StagingPool::Update(VmaAllocator allocator)
    {
        const std::scoped_lock lock{m_mutex};

        if (m_stagingBuffers.size() <= 1)
        {
            return;
        }

        for (auto iter = m_stagingBuffers.begin(); iter != m_stagingBuffers.end(); )
        {
            const bool isVirtualBlockEmpty            = vmaIsVirtualBlockEmpty(iter->virtualBlock);
            const bool areAllocationsEmpty            = iter->allocations.empty();
            const bool hasAtLeastOneStagingBufferLeft = m_stagingBuffers.size() > 1;

            if (isVirtualBlockEmpty && areAllocationsEmpty && hasAtLeastOneStagingBufferLeft)
            {
                iter->buffer.Destroy(allocator);
                
                vmaDestroyVirtualBlock(iter->virtualBlock);

                iter = m_stagingBuffers.erase(iter);
            }
            else if (areAllocationsEmpty ^ isVirtualBlockEmpty)
            {
                Logger::Error
                (
                    "Huh. How did we get here... Invalid allocation tracking! [Buffer={}] [Block={}]\n",
                    reinterpret_cast<void*>(iter->buffer.handle),
                    reinterpret_cast<void*>(iter->virtualBlock)
                );
            }
            else
            {
                ++iter;
            }
        }
    }

    StagingMemoryBlock StagingPool::Allocate
    (
        VkDevice device,
        VmaAllocator allocator,
        VkDeviceSize size,
        VkDeviceSize alignment
    )
    {
        constexpr VkDeviceSize STAGING_BUFFER_SIZE = Util::MiB(128ull);

        const std::scoped_lock lock{m_mutex};

        if (size == 0)
        {
            Logger::Error("{}\n", "Size must be non-zero!");
        }

        if (alignment != 0 && !Util::IsPowerOfTwo(alignment))
        {
            Logger::Error("Alignment must be a power of two! [Alignment={}]", alignment);
        }

        // Allocation attempt #1
        if (const auto stagingMemoryBlock = TryToAllocate(size, alignment); stagingMemoryBlock.has_value())
        {
            return stagingMemoryBlock.value();
        }

        // Create new staging buffer
        {
            // Allocate a dedicated staging buffer if size exceeds `STAGING_BUFFER_SIZE`
            const VkDeviceSize stagingBufferSize = std::max(STAGING_BUFFER_SIZE, Util::Align(size, alignment));

            const auto buffer = Vk::Buffer
            (
                device,
                allocator,
                stagingBufferSize,
                0,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            const VmaVirtualBlockCreateInfo createInfo =
            {
                .size                 = buffer.size,
                .flags                = 0,
                .pAllocationCallbacks = nullptr
            };

            VmaVirtualBlock virtualBlock = VK_NULL_HANDLE;

            Vk::CheckResult(vmaCreateVirtualBlock(&createInfo, &virtualBlock), "Failed to create virtual block!");

            m_stagingBuffers.emplace_back(buffer, virtualBlock);

            Vk::SetDebugName(device, buffer.handle, fmt::format("StagingPool/StagingBuffer/{}", m_stagingBuffers.size() - 1));
        }

        // Allocation attempt #2
        {
            auto& stagingBuffer = m_stagingBuffers.back();

            const auto stagingMemoryBlock = stagingBuffer.Allocate(size, alignment);

            if (stagingMemoryBlock.has_value())
            {
                return stagingMemoryBlock.value();
            }
        }

        Logger::Error("Failed to allocate staging block! [Size={}] [Alignment={}]\n", size, alignment);
    }

    void StagingPool::Free(const StagingMemoryBlock& stagingMemoryBlock)
    {
        const std::scoped_lock lock{m_mutex};

        for (auto& stagingBuffer : m_stagingBuffers)
        {
            if (stagingBuffer.buffer.handle != stagingMemoryBlock.buffer)
            {
                continue;
            }

            for (auto iter = stagingBuffer.allocations.begin(); iter != stagingBuffer.allocations.end(); ++iter)
            {
                if (iter->offset != stagingMemoryBlock.memoryBlock.offset)
                {
                    continue;
                }

                vmaVirtualFree(stagingBuffer.virtualBlock, iter->handle);
                stagingBuffer.allocations.erase(iter);

                return;
            }

            Logger::Error
            (
                "Allocation not found in matching buffer! [Buffer={}] [Offset={}] [Size={}]",
                reinterpret_cast<void*>(stagingMemoryBlock.buffer),
                stagingMemoryBlock.memoryBlock.offset,
                stagingMemoryBlock.memoryBlock.size
            );
        }

        Logger::Error
        (
            "Allocation not found! [Buffer={}] [Offset={}] [Size={}]",
            reinterpret_cast<void*>(stagingMemoryBlock.buffer),
            stagingMemoryBlock.memoryBlock.offset,
            stagingMemoryBlock.memoryBlock.size
        );
    }

    std::optional<StagingMemoryBlock> StagingPool::TryToAllocate(VkDeviceSize size, VkDeviceSize alignment)
    {
        for (auto& stagingBuffer : m_stagingBuffers)
        {
            const auto allocation = stagingBuffer.Allocate(size, alignment);

            if (allocation.has_value())
            {
                return allocation;
            }
        }

        return std::nullopt;
    }

    void StagingPool::Destroy(VmaAllocator allocator)
    {
        for (auto& stagingBuffer : m_stagingBuffers)
        {
            if (!stagingBuffer.allocations.empty())
            {
                Logger::Warning
                (
                    "Not all allocations were freed! [Buffer={}] [Virtual Block={}] [Allocation Count={}]",
                    reinterpret_cast<void*>(stagingBuffer.buffer.handle),
                    reinterpret_cast<void*>(stagingBuffer.virtualBlock),
                    stagingBuffer.allocations.size()
                );
            }

            stagingBuffer.buffer.Destroy(allocator);
            vmaDestroyVirtualBlock(stagingBuffer.virtualBlock);
        }
    }

    std::optional<StagingMemoryBlock> StagingPool::StagingBuffer::Allocate(VkDeviceSize size, VkDeviceSize alignment)
    {
        StagingPool::VirtualAllocation allocation = {};

        const VmaVirtualAllocationCreateInfo createInfo =
        {
            .size      = size,
            .alignment = alignment,
            .flags     = VMA_VIRTUAL_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT,
            .pUserData = nullptr
        };

        const VkResult result = vmaVirtualAllocate
        (
            virtualBlock,
            &createInfo,
            &allocation.handle,
            &allocation.offset
        );

        if (result == VK_SUCCESS)
        {
            allocations.emplace_back(allocation);

            return Vk::StagingMemoryBlock
            {
                .buffer      = buffer.handle,
                .hostAddress = static_cast<u8*>(buffer.hostAddress) + allocation.offset,
                .memoryBlock = Vk::MemoryBlock{
                    .offset = allocation.offset,
                    .size   = size
                }
            };
        }

        return std::nullopt;
    }
}