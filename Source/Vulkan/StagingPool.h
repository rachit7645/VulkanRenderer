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

#ifndef STAGING_POOL_H
#define STAGING_POOL_H

#include <vulkan/vulkan.h>

#include "Buffer.h"
#include "MemoryBlock.h"
#include "BarrierWriter.h"
#include "Externals/VMA.h"

namespace Vk
{
    struct StagingMemoryBlock
    {
        VkBuffer        buffer      = VK_NULL_HANDLE;
        void*           hostAddress = nullptr;
        Vk::MemoryBlock memoryBlock = {};
    };

    class StagingPool
    {
    public:
        void Update(VmaAllocator allocator);

        StagingMemoryBlock Allocate
        (
            VkDevice device,
            VmaAllocator allocator,
            VkDeviceSize size,
            VkDeviceSize alignment
        );

        void Free(const StagingMemoryBlock& stagingMemoryBlock);

        void Destroy(VmaAllocator allocator);
    private:
        struct VirtualAllocation
        {
            VmaVirtualAllocation handle = VK_NULL_HANDLE;
            VkDeviceSize         offset = 0;
        };

        struct StagingBuffer
        {
            Vk::Buffer                     buffer       = {};
            VmaVirtualBlock                virtualBlock = VK_NULL_HANDLE;
            std::vector<VirtualAllocation> allocations  = {};
        };

        std::optional<StagingMemoryBlock> TryToAllocate(VkDeviceSize size, VkDeviceSize alignment);

        std::vector<StagingBuffer> m_stagingBuffers;

        std::mutex m_mutex;
    };
}

#endif
