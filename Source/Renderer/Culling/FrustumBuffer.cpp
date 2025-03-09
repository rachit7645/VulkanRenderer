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
#include "Util/Plane.h"

namespace Renderer::Culling
{
    FrustumBuffer::FrustumBuffer(VkDevice device, VmaAllocator allocator)
    {
        for (usize i = 0; i < buffers.size(); ++i)
        {
            buffers[i] = Vk::Buffer
            (
                allocator,
                6 * sizeof(Maths::Plane),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            buffers[i].GetDeviceAddress(device);

            Vk::SetDebugName(device, buffers[i].handle, fmt::format("FrustumBuffer/{}", i));
        }
    }

    void FrustumBuffer::LoadPlanes
    (
        usize FIF,
        const Vk::CommandBuffer& cmdBuffer,
        const glm::mat4& projectionView
    )
    {
        const auto planes = Maths::ExtractFrustumPlanes(projectionView);

        const VkDeviceSize size = planes.size() * sizeof(Maths::Plane);

        buffers[FIF].Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            0,
            size
        );

        vkCmdUpdateBuffer
        (
            cmdBuffer.handle,
            buffers[FIF].handle,
            0,
            size,
            planes.data()
        );

        buffers[FIF].Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
            0,
            size
        );
    }

    void FrustumBuffer::Destroy(VmaAllocator allocator)
    {
        for (auto& buffer : buffers)
        {
            buffer.Destroy(allocator);
        }
    }
}
