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

#include "Buffer.h"
#include "Util/Log.h"
#include "Renderer/Vertex.h"

namespace Vk
{
    Buffer::Buffer
    (
        VkDevice device,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        const VkPhysicalDeviceMemoryProperties& memProperties
    )
        : size(size),
          usage(usage)
    {
        // Creation info
        VkBufferCreateInfo createInfo =
        {
            .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .size                  = size,
            .usage                 = usage,
            .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices   = nullptr
        };

        // Create buffer
        if (vkCreateBuffer(
                device,
                &createInfo,
                nullptr,
                &handle
            ) != VK_SUCCESS)
        {
            // Log
            Logger::Error("Failed to create vertex buffer! [device={}]\n", reinterpret_cast<void*>(device));
        }

        // Log
        Logger::Info("Created buffer! [handle={}]\n", reinterpret_cast<void*>(handle));

        // Memory requirements
        VkMemoryRequirements memRequirements = {};
        vkGetBufferMemoryRequirements(device, handle, &memRequirements);

        // Allocation info
        VkMemoryAllocateInfo allocInfo =
        {
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext           = nullptr,
            .allocationSize  = memRequirements.size,
            .memoryTypeIndex = FindMemoryType(
            memRequirements.memoryTypeBits,
                properties,
                memProperties
            )
        };

        // Allocate
        if (vkAllocateMemory(
                device,
                &allocInfo,
                nullptr,
                &memory
            ) != VK_SUCCESS)
        {
            // Log
            Logger::Error
            (
                "Failed to allocate vertex buffer memory! [device={}] [buffer={}]\n",
                reinterpret_cast<void*>(device),
                reinterpret_cast<void*>(handle)
            );
        }

        // Attach memory to buffer
        vkBindBufferMemory(device, handle, memory, 0);

        // Log
        Logger::Debug
        (
            "Allocated memory for buffer! [buffer={}] [size={}]\n",
            reinterpret_cast<void*>(handle),
            memRequirements.size
        );
    }

    void Buffer::Map(VkDevice device, VkDeviceSize offset, VkDeviceSize rangeSize)
    {
        // Map
        vkMapMemory(device, memory, offset, rangeSize, 0, &mappedPtr);
    }

    template <typename T>
    void Buffer::LoadData(VkDevice device, const std::span<const T> data)
    {
        // Map
        Map(device);
        // Copy
        std::memcpy(mappedPtr, data.data(), size);
        // Unmap
        Unmap(device);
    }

    void Buffer::CopyBuffer
    (
        Vk::Buffer srcBuffer,
        Vk::Buffer dstBuffer,
        VkDeviceSize copySize,
        VkDevice device,
        VkCommandPool commandPool,
        VkQueue queue
    )
    {
        // Transfer buffer
        VkCommandBufferAllocateInfo allocInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext              = nullptr,
            .commandPool        = commandPool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        // Create buffer
        VkCommandBuffer transferCmdBuffer = {};
        vkAllocateCommandBuffers(device, &allocInfo, &transferCmdBuffer);

        // Begin info
        VkCommandBufferBeginInfo beginInfo =
        {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext            = nullptr,
            .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr
        };

        // Begin
        vkBeginCommandBuffer(transferCmdBuffer, &beginInfo);

        // Copy region
        VkBufferCopy copyRegion =
        {
            .srcOffset = 0,
            .dstOffset = 0,
            .size      = copySize
        };

        // Copy
        vkCmdCopyBuffer(transferCmdBuffer, srcBuffer.handle, dstBuffer.handle, 1, &copyRegion);
        // End
        vkEndCommandBuffer(transferCmdBuffer);

        // Submit info
        VkSubmitInfo submitInfo =
        {
            .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext                = nullptr,
            .waitSemaphoreCount   = 0,
            .pWaitSemaphores      = nullptr,
            .pWaitDstStageMask    = nullptr,
            .commandBufferCount   = 1,
            .pCommandBuffers      = &transferCmdBuffer,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores    = nullptr
        };

        // Submit
        vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue);

        // Cleanup
        vkFreeCommandBuffers(device, commandPool, 1, &transferCmdBuffer);
    }

    void Buffer::DeleteBuffer(VkDevice device)
    {
        // Log
        Logger::Debug
        (
            "Destroying buffer [buffer={}] [memory={}]\n",
            reinterpret_cast<void*>(handle),
            reinterpret_cast<void*>(memory)
        );
        // Destroy
        vkDestroyBuffer(device, handle, nullptr);
        // Free
        vkFreeMemory(device, memory, nullptr);
    }

    u32 Buffer::FindMemoryType
    (
        u32 typeFilter,
        VkMemoryPropertyFlags properties,
        const VkPhysicalDeviceMemoryProperties& memProperties
    )
    {
        // Search through memory types
        for (u32 i = 0; i < memProperties.memoryTypeCount; ++i)
        {
            // Type check
            bool typeCheck = typeFilter & (0x1 << i);
            // Property check
            bool propertyCheck = (memProperties.memoryTypes[i].propertyFlags & properties) == properties;
            // Check
            if (typeCheck && propertyCheck)
            {
                // Found memory!
                return i;
            }
        }

        // If we didn't find memory, terminate
        Logger::Error("Failed to find suitable memory type! [buffer={}]\n", reinterpret_cast<void*>(handle));
    }

    void Buffer::Unmap(VkDevice device)
    {
        // Unmap
        vkUnmapMemory(device, memory);
        // Fix mapped pointer
        mappedPtr = nullptr;
    }

    // Explicit template initialisations
    template void Buffer::LoadData(VkDevice, std::span<const Renderer::Vertex>);
    template void Buffer::LoadData(VkDevice, std::span<const f32>);
    template void Buffer::LoadData(VkDevice, std::span<const u16>);
    template void Buffer::LoadData(VkDevice, std::span<const u32>);
}