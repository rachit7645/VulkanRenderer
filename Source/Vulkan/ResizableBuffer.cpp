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

#include "ResizableBuffer.h"

#include "Util/Enum.h"
#include "Vulkan/DebugUtils.h"

namespace Vk
{
    ResizableBuffer::ResizableBuffer(const ResizableBufferFlags flags)
        : flags(flags)
    {
    }

    void ResizableBuffer::Reserve
    (
        VkDevice device,
        VmaAllocator allocator,
        const Vk::CommandBuffer& cmdBuffer,
        VkDeviceSize capacity,
        Util::DeletionQueue& deletionQueue
    )
    {
        if (capacity <= buffer.size)
        {
            return;
        }

        const bool copyOnResize = (flags & ResizableBufferFlags::CopyOnResize) == ResizableBufferFlags::CopyOnResize;

        auto oldBuffer = buffer;

        deletionQueue.Push([allocator, buffer = oldBuffer] () mutable
        {
           buffer.Destroy(allocator);
        });

        // TODO: Move choice of buffer creation parameters to user

        VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

        if (copyOnResize)
        {
            usageFlags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        }

        buffer = Vk::Buffer
        (
            device,
            allocator,
            capacity,
            0,
            usageFlags,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        );

        if (!copyOnResize)
        {
            return;
        }

        if (oldBuffer.handle == VK_NULL_HANDLE || oldBuffer.size == 0)
        {
            return;
        }

        Vk::BeginLabel(cmdBuffer, "Resizable Buffer -> Copy", {0.3882f, 0.8294f, 0.2118f, 1.0f});

        const VkBufferCopy2 copyRegion =
        {
            .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
            .pNext     = nullptr,
            .srcOffset = 0,
            .dstOffset = 0,
            .size      = oldBuffer.size
        };

        const VkCopyBufferInfo2 copyInfo =
        {
            .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
            .pNext       = nullptr,
            .srcBuffer   = oldBuffer.handle,
            .dstBuffer   = buffer.handle,
            .regionCount = 1,
            .pRegions    = &copyRegion,
        };

        oldBuffer.Barrier
        (
            cmdBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                .dstAccessMask  = VK_ACCESS_2_TRANSFER_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = oldBuffer.size
            }
        );

        vkCmdCopyBuffer2(cmdBuffer.handle, &copyInfo);

        buffer.Barrier
        (
            cmdBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                .srcAccessMask  = VK_ACCESS_2_TRANSFER_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = oldBuffer.size
            }
        );

        Vk::EndLabel(cmdBuffer);
    }

    void ResizableBuffer::Destroy(VmaAllocator allocator)
    {
        buffer.Destroy(allocator);
    }
}
