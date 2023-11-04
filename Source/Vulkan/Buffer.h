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
            VkDevice device,
            VkDeviceSize size,
            VkBufferUsageFlags usage,
            VkMemoryPropertyFlags properties,
            const VkPhysicalDeviceMemoryProperties& memProperties
        );

        // Load data
        template <typename T>
        void LoadData(VkDevice device, const std::span<const T> data);

        // Copy from src to dst buffer
        static void CopyBuffer
        (
            Vk::Buffer srcBuffer,
            Vk::Buffer dstBuffer,
            VkDeviceSize copySize,
            VkDevice device,
            VkCommandPool commandPool,
            VkQueue queue
        );

        // Internal deletion function
        void DeleteBuffer(VkDevice device);

        // Buffer handle
        VkBuffer handle = {};
        // Memory
        VkDeviceMemory memory = {};
        // Buffer size
        VkDeviceSize size = {};
        // Buffer usage flags
        VkBufferUsageFlags usage = {};
    private:
        // Find memory type
        u32 FindMemoryType
        (
            u32 typeFilter,
            VkMemoryPropertyFlags properties,
            const VkPhysicalDeviceMemoryProperties& memProperties
        );
    };
}

#endif
