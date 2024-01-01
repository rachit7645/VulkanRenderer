/*
 *    Copyright 2023 Rachit Khandelwal
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

#include <span>
#include <vulkan/vulkan.h>
#include "Util/Util.h"
#include "Context.h"

namespace Vk
{
    class Buffer
    {
    public:
        // Default constructor (to make C++ happy)
        Buffer() = default;

        // Creates the buffer
        Buffer
        (
            VmaAllocator allocator,
            VkDeviceSize size,
            VkBufferUsageFlags usage,
            VkMemoryPropertyFlags properties,
            VmaAllocationCreateFlags allocationFlags,
            VmaMemoryUsage memoryUsage
        );

        // Map buffer
        void Map(VmaAllocator allocator);
        // Unmap buffer
        void Unmap(VmaAllocator allocator) const;

        // Load data
        template <typename T>
        void LoadData(VmaAllocator allocator, const std::span<const T> data);

        // Copy from src to dst buffer
        static void CopyBuffer
        (
            const std::shared_ptr<Vk::Context>& context,
            Vk::Buffer& srcBuffer,
            Vk::Buffer& dstBuffer,
            VkDeviceSize copySize
        );

        // Internal deletion function
        void Destroy(VmaAllocator allocator) const;

        // Vulkan handles
        VkBuffer      handle     = VK_NULL_HANDLE;
        VmaAllocation allocation = {};

        // Buffer info
        VmaAllocationInfo allocInfo  = {};
    };
}

#endif
