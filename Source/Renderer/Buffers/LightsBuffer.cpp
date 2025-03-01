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

#include "LightsBuffer.h"

#include "Vulkan/DebugUtils.h"
#include "Util/Log.h"

namespace Renderer::Buffers
{
    LightsBuffer::LightsBuffer(VkDevice device, VmaAllocator allocator)
    {
        constexpr VkDeviceSize LIGHT_BUFFER_SIZE =
            sizeof(u32) + Objects::MAX_DIR_LIGHT_COUNT   * sizeof(Objects::DirLight)   +
            sizeof(u32) + Objects::MAX_POINT_LIGHT_COUNT * sizeof(Objects::PointLight) +
            sizeof(u32) + Objects::MAX_SPOT_LIGHT_COUNT  * sizeof(Objects::SpotLight);

        for (usize i = 0; i < buffers.size(); ++i)
        {
            buffers[i] = Vk::Buffer
            (
                allocator,
                LIGHT_BUFFER_SIZE,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            const u32  count       = 0;
            const auto pMappedData = static_cast<u8*>(buffers[i].allocationInfo.pMappedData);

            std::memcpy(pMappedData + GetDirLightOffset(),   &count, sizeof(u32));
            std::memcpy(pMappedData + GetPointLightOffset(), &count, sizeof(u32));
            std::memcpy(pMappedData + GetSpotLightOffset(),  &count, sizeof(u32));

            if (!(buffers[i].memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
            {
                Vk::CheckResult(vmaFlushAllocation(
                    allocator,
                    buffers[i].allocation,
                    0,
                    buffers[i].requestedSize),
                    "Failed to flush allocation!"
                );
            }

            buffers[i].GetDeviceAddress(device);

            Vk::SetDebugName(device, buffers[i].handle, fmt::format("LightBuffer/{}", i));
        }
    }

    void LightsBuffer::WriteLights
    (
        usize FIF,
        VmaAllocator allocator,
        const std::span<const Objects::DirLight> dirLights,
        const std::span<const Objects::PointLight> pointLights,
        const std::span<const Objects::SpotLight> spotLights
    )
    {
        WriteLights(FIF, GetDirLightOffset(),   dirLights);
        WriteLights(FIF, GetPointLightOffset(), pointLights);
        WriteLights(FIF, GetSpotLightOffset(),  spotLights);

        if (!(buffers[FIF].memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            Vk::CheckResult(vmaFlushAllocation(
                allocator,
                buffers[FIF].allocation,
                0,
                buffers[FIF].requestedSize),
                "Failed to flush allocation!"
            );
        }
    }

    template <typename LightType>
    void LightsBuffer::WriteLights
    (
        usize FIF,
        VkDeviceSize offset,
        std::span<const LightType> lights
    )
    {
        const auto requiredSize  = lights.size_bytes();
        const auto availableSize = ((buffers[FIF].requestedSize - (offset + sizeof(u32))) / sizeof(LightType)) * sizeof(LightType);

        const auto size = std::min(requiredSize, availableSize);

        if (size == 0 || lights.empty())
        {
            return;
        }

        const auto pMappedData = static_cast<u8*>(buffers[FIF].allocationInfo.pMappedData) + offset;
        const u32  count       = size / sizeof(LightType);

        std::memcpy
        (
            pMappedData,
            &count,
            sizeof(u32)
        );

        std::memcpy
        (
            pMappedData + sizeof(u32),
            lights.data(),
            size
        );
    }

    VkDeviceSize LightsBuffer::GetDirLightOffset()
    {
        return 0;
    }

    VkDeviceSize LightsBuffer::GetPointLightOffset()
    {
        return sizeof(u32) + Objects::MAX_DIR_LIGHT_COUNT * sizeof(Objects::DirLight);
    }

    VkDeviceSize LightsBuffer::GetSpotLightOffset()
    {
        return sizeof(u32) + Objects::MAX_DIR_LIGHT_COUNT   * sizeof(Objects::DirLight) +
               sizeof(u32) + Objects::MAX_POINT_LIGHT_COUNT * sizeof(Objects::PointLight);
    }

    void LightsBuffer::Destroy(VmaAllocator allocator)
    {
        for (auto& buffer : buffers)
        {
            buffer.Destroy(allocator);
        }
    }
}
