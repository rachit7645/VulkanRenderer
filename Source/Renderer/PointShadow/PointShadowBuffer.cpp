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

#include "PointShadowBuffer.h"

#include "Vulkan/DebugUtils.h"
#include "Util/Log.h"
#include "Renderer/Objects/Lights.h"

namespace Renderer::PointShadow
{
    PointShadowBuffer::PointShadowBuffer(VkDevice device, VmaAllocator allocator)
    {
        for (usize i = 0; i < buffers.size(); ++i)
        {
            buffers[i] = Vk::Buffer
            (
                allocator,
                sizeof(glm::vec2) + sizeof(PointShadow::PointShadowBuffer) * Objects::MAX_POINT_LIGHT_COUNT,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            buffers[i].GetDeviceAddress(device);

            Vk::SetDebugName(device, buffers[i].handle, fmt::format("PointShadowBuffer/{}", i));
        }
    }

    void PointShadowBuffer::LoadPointShadowData
    (
        usize FIF,
        VmaAllocator allocator,
        const glm::vec2& shadowPlanes,
        const std::span<const PointShadow::PointShadowData> pointShadows
    )
    {
        auto pMappedData = static_cast<u8*>(buffers[FIF].allocationInfo.pMappedData);

        std::memcpy
        (
            pMappedData,
            glm::value_ptr(shadowPlanes),
            sizeof(glm::vec2)
        );

        std::memcpy
        (
            pMappedData + sizeof(glm::vec2),
            pointShadows.data(),
            pointShadows.size_bytes()
        );

        if (!(buffers[FIF].memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            Vk::CheckResult(vmaFlushAllocation(
                allocator,
                buffers[FIF].allocation,
                0,
                sizeof(glm::vec2) + pointShadows.size_bytes()),
                "Failed to flush allocation!"
            );
        }
    }

    void PointShadowBuffer::Destroy(VmaAllocator allocator)
    {
        for (auto& buffer : buffers)
        {
            buffer.Destroy(allocator);
        }
    }
}
