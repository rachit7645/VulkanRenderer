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

#include "Buffer.h"

#include <volk/volk.h>

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
        const VkBufferCreateInfo createInfo =
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

        const VmaAllocationCreateInfo allocCreateInfo =
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

        Vk::CheckResult(vmaCreateBuffer(
            allocator,
            &createInfo,
            &allocCreateInfo,
            &handle,
            &allocation,
            &allocInfo),
            "Failed to create vertex buffer!"
        );

        Logger::Debug("Created buffer! [handle={}]\n", std::bit_cast<void*>(handle));
    }

    void Buffer::Map(VmaAllocator allocator)
    {
        vmaMapMemory(allocator, allocation, &allocInfo.pMappedData);
    }

    void Buffer::Unmap(VmaAllocator allocator)
    {
        vmaUnmapMemory(allocator, allocation);
    }

    void Buffer::GetDeviceAddress(VkDevice device)
    {
        const VkBufferDeviceAddressInfo bdaInfo =
        {
            .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .pNext  = nullptr,
            .buffer = handle
        };

        deviceAddress = vkGetBufferDeviceAddress(device, &bdaInfo);
    }

    void Buffer::Barrier
    (
        const Vk::CommandBuffer& cmdBuffer,
        VkPipelineStageFlags2 srcStageMask,
        VkAccessFlags2 srcAccessMask,
        VkPipelineStageFlags2 dstStageMask,
        VkAccessFlags2 dstAccessMask,
        VkDeviceSize offset,
        VkDeviceSize size
    ) const
    {
        const VkBufferMemoryBarrier2 barrier =
        {
            .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .pNext               = nullptr,
            .srcStageMask        = srcStageMask,
            .srcAccessMask       = srcAccessMask,
            .dstStageMask        = dstStageMask,
            .dstAccessMask       = dstAccessMask,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer              = handle,
            .offset              = offset,
            .size                = size
        };

        const VkDependencyInfo dependencyInfo =
        {
            .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext                    = nullptr,
            .dependencyFlags          = 0,
            .memoryBarrierCount       = 0,
            .pMemoryBarriers          = nullptr,
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers    = &barrier,
            .imageMemoryBarrierCount  = 0,
            .pImageMemoryBarriers     = nullptr
        };

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencyInfo);
    }

    void Buffer::Destroy(VmaAllocator allocator) const
    {
        Logger::Debug
        (
            "Destroying buffer [buffer={}] [allocation={}]\n",
            std::bit_cast<void*>(handle),
            std::bit_cast<void*>(allocation)
        );

        vmaDestroyBuffer(allocator, handle, allocation);
    }
}