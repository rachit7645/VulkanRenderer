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
#include "Util.h"

namespace Vk
{
    Buffer::Buffer
    (
        const std::shared_ptr<Vk::Context>& context,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties
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
                context->device,
                &createInfo,
                nullptr,
                &handle
            ) != VK_SUCCESS)
        {
            // Log
            Logger::Error("Failed to create vertex buffer! [device={}]\n",
                reinterpret_cast<void*>(context->device)
            );
        }

        // Memory requirements
        VkMemoryRequirements memRequirements = {};
        vkGetBufferMemoryRequirements(context->device, handle, &memRequirements);

        // Allocation info
        VkMemoryAllocateInfo allocInfo =
        {
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext           = nullptr,
            .allocationSize  = memRequirements.size,
            .memoryTypeIndex = Vk::FindMemoryType(
                memRequirements.memoryTypeBits,
                properties,
                context->phyMemProperties
            )
        };

        // Allocate
        if (vkAllocateMemory(
                context->device,
                &allocInfo,
                nullptr,
                &memory
            ) != VK_SUCCESS)
        {
            // Log
            Logger::Error
            (
                "Failed to allocate vertex buffer memory! [device={}] [buffer={}]\n",
                reinterpret_cast<void*>(context->device),
                reinterpret_cast<void*>(handle)
            );
        }

        // Attach memory to buffer
        vkBindBufferMemory(context->device, handle, memory, 0);

        // Log
        Logger::Info("Created buffer! [handle={}]\n", reinterpret_cast<void*>(handle));
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
        const std::shared_ptr<Vk::Context>& context,
        Vk::Buffer& srcBuffer,
        Vk::Buffer& dstBuffer,
        VkDeviceSize copySize
    )
    {
        Vk::SingleTimeCmdBuffer(context, [&] (VkCommandBuffer cmdBuffer)
        {
            // Copy region
            VkBufferCopy copyRegion =
            {
                .srcOffset = 0,
                .dstOffset = 0,
                .size      = copySize
            };
            // Copy
            vkCmdCopyBuffer
            (
                cmdBuffer,
                srcBuffer.handle,
                dstBuffer.handle,
                1,
                &copyRegion
            );
        });
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
    template void Buffer::LoadData(VkDevice, std::span<const u8>);
    template void Buffer::LoadData(VkDevice, std::span<const u16>);
    template void Buffer::LoadData(VkDevice, std::span<const u32>);
}