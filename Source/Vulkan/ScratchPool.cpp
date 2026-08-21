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

#include "ScratchPool.h"

#include "Util/Memory.h"
#include "Vulkan/DebugUtils.h"
#include "Vulkan/Util.h"

namespace Vk
{
    VkDeviceAddress ScratchPool::Allocate
    (
        VkDevice device,
        VmaAllocator allocator,
        VkDeviceSize size,
        VkDeviceSize alignment
    )
    {
        constexpr VkDeviceSize SCRATCH_BUFFER_SIZE = Util::MiB(16ull);

        if (size == 0)
        {
            Logger::Error("{}\n", "Size must be non-zero!");
        }

        if (alignment != 0 && !Util::IsPowerOfTwo(alignment))
        {
            Logger::Error("Alignment must be a power of two! [Alignment={}]", alignment);
        }

        // Allocation Attempt #1
        for (const auto& scratchBuffer : m_scratchBuffers)
        {
            const VmaVirtualAllocationCreateInfo createInfo =
            {
                .size      = size,
                .alignment = alignment,
                .flags     = VMA_VIRTUAL_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT,
                .pUserData = nullptr
            };

            VmaVirtualAllocation handle = VK_NULL_HANDLE;
            VkDeviceSize         offset = 0;

            const VkResult result = vmaVirtualAllocate
            (
                scratchBuffer.virtualBlock,
                &createInfo,
                &handle,
                &offset
            );

            if (result == VK_SUCCESS)
            {
                return scratchBuffer.buffer.deviceAddress + offset;
            }
        }

        // Allocate new buffer
        {
            const VkDeviceSize scratchBufferSize = std::max(SCRATCH_BUFFER_SIZE, Util::Align(size, alignment));

            auto buffer = Vk::Buffer
            (
                device,
                allocator,
                scratchBufferSize,
                alignment,
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                0,
                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
            );

            const VmaVirtualBlockCreateInfo virtualBlockCreateInfo =
            {
                .size                 = buffer.size,
                .flags                = VMA_VIRTUAL_BLOCK_CREATE_LINEAR_ALGORITHM_BIT,
                .pAllocationCallbacks = nullptr
            };

            VmaVirtualBlock virtualBlock = VK_NULL_HANDLE;

            Vk::CheckResult(vmaCreateVirtualBlock(&virtualBlockCreateInfo, &virtualBlock), "Failed to create virtual block!");

            m_scratchBuffers.emplace_back(buffer, virtualBlock);

            Vk::SetDebugName(device, buffer.handle, fmt::format("ScratchPool/ScratchBuffer/{}", m_scratchBuffers.size() - 1));
        }

        // Allocation Attempt #2
        {
            const auto& [buffer, virtualBlock] = m_scratchBuffers.back();

            const VmaVirtualAllocationCreateInfo allocationCreateInfo =
            {
                .size      = size,
                .alignment = alignment,
                .flags     = VMA_VIRTUAL_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT,
                .pUserData = nullptr
            };

            VmaVirtualAllocation handle = VK_NULL_HANDLE;
            VkDeviceSize         offset = 0;

            const VkResult result = vmaVirtualAllocate
            (
                virtualBlock,
                &allocationCreateInfo,
                &handle,
                &offset
            );

            if (result == VK_SUCCESS)
            {
                return buffer.deviceAddress + offset;
            }
        }

        Logger::Error("Failed to allocate scratch memory! [Size={}] [Alignment={}]\n", size, alignment);
    }

    void ScratchPool::Destroy(VmaAllocator allocator)
    {
        for (auto& scratchBuffer : m_scratchBuffers)
        {
            scratchBuffer.buffer.Destroy(allocator);

            vmaClearVirtualBlock(scratchBuffer.virtualBlock);
            vmaDestroyVirtualBlock(scratchBuffer.virtualBlock);
        }
    }
}
