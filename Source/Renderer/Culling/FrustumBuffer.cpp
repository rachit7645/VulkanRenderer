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

#include "FrustumBuffer.h"

#include "Vulkan/DebugUtils.h"
#include "Util/Log.h"
#include "GPU/Plane.h"

namespace Renderer::Culling
{
    FrustumBuffer::FrustumBuffer(VkDevice device, VmaAllocator allocator)
    {
        buffer = Vk::Buffer
        (
            allocator,
            sizeof(GPU::FrustumBuffer),
            0,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            VMA_MEMORY_USAGE_AUTO
        );

        buffer.GetDeviceAddress(device);

        Vk::SetDebugName(device, buffer.handle, "FrustumBuffer");
    }

    void FrustumBuffer::Load(const Vk::CommandBuffer& cmdBuffer, const glm::mat4& projectionView)
    {
        const auto frustum = GPU::FrustumBuffer(projectionView);

        buffer.Barrier
        (
            cmdBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .dstAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = sizeof(GPU::FrustumBuffer)
            }
        );

        vkCmdUpdateBuffer
        (
            cmdBuffer.handle,
            buffer.handle,
            0,
            sizeof(GPU::FrustumBuffer),
            &frustum
        );

        buffer.Barrier
        (
            cmdBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = sizeof(GPU::FrustumBuffer)
            }
        );
    }

    void FrustumBuffer::Destroy(VmaAllocator allocator)
    {
        buffer.Destroy(allocator);
    }
}