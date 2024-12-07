/*
 *    Copyright 2023 - 2024 Rachit Khandelwal
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef BUFFER_H
#define BUFFER_H

#include <vulkan/vulkan.h>

#include "Context.h"

namespace Vk
{
    class Buffer
    {
    public:
        // WARNING: INVALID STATE!
        Buffer() = default;

        Buffer
        (
            VmaAllocator allocator,
            VkDeviceSize size,
            VkBufferUsageFlags usage,
            VkMemoryPropertyFlags properties,
            VmaAllocationCreateFlags allocationFlags,
            VmaMemoryUsage memoryUsage
        );

        void Map(VmaAllocator allocator);
        void Unmap(VmaAllocator allocator) const;

        void GetDeviceAddress(VkDevice device);
        void Destroy(VmaAllocator allocator) const;

        // Vulkan handles
        VkBuffer        handle        = VK_NULL_HANDLE;
        VmaAllocation   allocation    = {};
        VkDeviceAddress deviceAddress = 0;

        // Buffer allocation info
        VmaAllocationInfo allocInfo = {};
    };
}

#endif
