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
        : requestedSize(size)
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
            &allocationInfo),
            "Failed to create buffer!"
        );

        vmaGetMemoryTypeProperties(allocator, allocationInfo.memoryType, &memoryProperties);
    }

    void Buffer::GetDeviceAddress(VkDevice device)
    {
        if (handle == VK_NULL_HANDLE)
        {
            return;
        }

        const VkBufferDeviceAddressInfo bdaInfo =
        {
            .sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .pNext  = nullptr,
            .buffer = handle
        };

        deviceAddress = vkGetBufferDeviceAddress(device, &bdaInfo);
    }

    void Buffer::Barrier(const Vk::CommandBuffer& cmdBuffer, const Vk::BufferBarrier& barrier) const
    {
        if (handle == VK_NULL_HANDLE)
        {
            return;
        }

        const VkBufferMemoryBarrier2 bufferBarrier =
        {
            .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .pNext               = nullptr,
            .srcStageMask        = barrier.srcStageMask,
            .srcAccessMask       = barrier.srcAccessMask,
            .dstStageMask        = barrier.dstStageMask,
            .dstAccessMask       = barrier.dstAccessMask,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer              = handle,
            .offset              = barrier.offset,
            .size                = barrier.size
        };

        const VkDependencyInfo dependencyInfo =
        {
            .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext                    = nullptr,
            .dependencyFlags          = 0,
            .memoryBarrierCount       = 0,
            .pMemoryBarriers          = nullptr,
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers    = &bufferBarrier,
            .imageMemoryBarrierCount  = 0,
            .pImageMemoryBarriers     = nullptr
        };

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencyInfo);
    }

    void Buffer::Destroy(VmaAllocator allocator)
    {
        if (handle == VK_NULL_HANDLE)
        {
            return;
        }

        vmaDestroyBuffer(allocator, handle, allocation);

        handle           = VK_NULL_HANDLE;
        allocation       = {};
        deviceAddress    = 0;
        requestedSize    = 0;
        allocationInfo   = {};
        memoryProperties = {};
    }
}