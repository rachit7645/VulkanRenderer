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
#include "Models/Vertex.h"
#include "Util.h"

namespace Vk
{
    Buffer::Buffer
    (
        VmaAllocator allocator,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VmaAllocationCreateFlags allocationFlags,
        VmaMemoryUsage memoryUsage
    )
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

        // Allocation info
        VmaAllocationCreateInfo allocCreateInfo =
        {
            .flags          = allocationFlags,
            .usage          = memoryUsage,
            .requiredFlags  = properties,
            .preferredFlags = 0,
            .memoryTypeBits = 0,
            .pool           = VK_NULL_HANDLE,
            .pUserData      = nullptr,
            .priority       = 0.0f
        };

        // Create buffer
        if (vmaCreateBuffer(
                allocator,
                &createInfo,
                &allocCreateInfo,
                &handle,
                &allocation,
                &allocInfo
            ) != VK_SUCCESS)
        {
            // Log
            Logger::Error("Failed to create vertex buffer! [allocator={}]\n",
                std::bit_cast<void*>(allocator)
            );
        }

        // Log
        Logger::Debug("Created buffer! [handle={}]\n", reinterpret_cast<void*>(handle));
    }

    void Buffer::Map(VmaAllocator allocator)
    {
        // Map
        vmaMapMemory(allocator, allocation, &allocInfo.pMappedData);
    }

    void Buffer::Unmap(VmaAllocator allocator) const
    {
        // Unmap
        vmaUnmapMemory(allocator, allocation);
    }

    template <typename T>
    void Buffer::LoadData(VmaAllocator allocator, const std::span<const T> data)
    {
        // Map
        Map(allocator);
        // Copy
        std::memcpy(allocInfo.pMappedData, data.data(), allocInfo.size);
        // Unmap
        Unmap(allocator);
    }

    void Buffer::CopyBuffer
    (
        const std::shared_ptr<Vk::Context>& context,
        Vk::Buffer& srcBuffer,
        Vk::Buffer& dstBuffer,
        VkDeviceSize copySize
    )
    {
        Vk::ImmediateSubmit(context, [&](const Vk::CommandBuffer& cmdBuffer)
        {
            // Copy region
            VkBufferCopy2 copyRegion =
            {
                .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                .pNext     = nullptr,
                .srcOffset = 0,
                .dstOffset = 0,
                .size      = copySize
            };

            // Copy info
            VkCopyBufferInfo2 copyInfo =
            {
                .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
                .pNext       = nullptr,
                .srcBuffer   = srcBuffer.handle,
                .dstBuffer   = dstBuffer.handle,
                .regionCount = 1,
                .pRegions    = &copyRegion
            };

            // Copy
            vkCmdCopyBuffer2(cmdBuffer.handle, &copyInfo);
        });
    }

    void Buffer::Destroy(VmaAllocator allocator) const
    {
        // Log
        Logger::Debug
        (
            "Destroying buffer [buffer={}] [allocation={}]\n",
            reinterpret_cast<void*>(handle),
            reinterpret_cast<void*>(allocation)
        );
        // Destroy
        vmaDestroyBuffer(allocator, handle, allocation);
    }

    // Explicit template initialisations
    template void Buffer::LoadData(VmaAllocator, std::span<const Models::Vertex>);
    template void Buffer::LoadData(VmaAllocator, std::span<const f32>);
    template void Buffer::LoadData(VmaAllocator, std::span<const u8>);
    template void Buffer::LoadData(VmaAllocator, std::span<const u32>);
}