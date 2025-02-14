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

namespace Renderer::Buffers
{
    LightsBuffer::LightsBuffer(VkDevice device, VmaAllocator allocator)
    {
        for (usize i = 0; i < dirLightBuffers.size(); ++i)
        {
            dirLightBuffers[i] = Vk::Buffer
            (
                allocator,
                static_cast<u32>(sizeof(u32) + Objects::MAX_DIR_LIGHT_COUNT * sizeof(Objects::DirLight)),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            const u32 count = 0;
            std::memcpy(dirLightBuffers[i].allocInfo.pMappedData, &count, sizeof(u32));

            dirLightBuffers[i].GetDeviceAddress(device);

            Vk::SetDebugName(device, dirLightBuffers[i].handle, fmt::format("DirLightBuffer/{}", i));
        }

        for (usize i = 0; i < pointLightBuffers.size(); ++i)
        {
            pointLightBuffers[i] = Vk::Buffer
            (
                allocator,
                static_cast<u32>(sizeof(u32) + Objects::MAX_POINT_LIGHT_COUNT * sizeof(Objects::PointLight)),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            const u32 count = 0;
            std::memcpy(pointLightBuffers[i].allocInfo.pMappedData, &count, sizeof(u32));

            pointLightBuffers[i].GetDeviceAddress(device);

            Vk::SetDebugName(device, pointLightBuffers[i].handle, fmt::format("PointLightBuffer/{}", i));
        }

        for (usize i = 0; i < spotLightBuffers.size(); ++i)
        {
            spotLightBuffers[i] = Vk::Buffer
            (
                allocator,
                static_cast<u32>(sizeof(u32) + Objects::MAX_SPOT_LIGHT_COUNT * sizeof(Objects::SpotLight)),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            const u32 count = 0;
            std::memcpy(spotLightBuffers[i].allocInfo.pMappedData, &count, sizeof(u32));

            spotLightBuffers[i].GetDeviceAddress(device);

            Vk::SetDebugName(device, spotLightBuffers[i].handle, fmt::format("SpotLightBuffer/{}", i));
        }
    }

    void LightsBuffer::WriteDirLights(usize FIF, const std::span<const Objects::DirLight> lights)
    {
        WriteLights(lights, dirLightBuffers[FIF]);
    }

    void LightsBuffer::WritePointLights(usize FIF, const std::span<const Objects::PointLight> lights)
    {
        WriteLights(lights, pointLightBuffers[FIF]);
    }

    void LightsBuffer::WriteSpotLights(usize FIF, const std::span<const Objects::SpotLight> lights)
    {
        WriteLights(lights, spotLightBuffers[FIF]);
    }

    template <typename LightType>
    void LightsBuffer::WriteLights(std::span<const LightType> lights, const Vk::Buffer& buffer)
    {
        const usize size  = std::min(buffer.requestedSize - sizeof(u32), lights.size_bytes());
        const usize count = size / sizeof(LightType);

        std::memcpy
        (
            buffer.allocInfo.pMappedData,
            &count,
            sizeof(u32)
        );

        std::memcpy
        (
            static_cast<u8*>(buffer.allocInfo.pMappedData) + sizeof(u32),
            lights.data(),
            size
        );
    }

    void LightsBuffer::Destroy(VmaAllocator allocator)
    {
        for (auto& buffer : dirLightBuffers)
        {
            buffer.Destroy(allocator);
        }

        for (auto& buffer : pointLightBuffers)
        {
            buffer.Destroy(allocator);
        }

        for (auto& buffer : spotLightBuffers)
        {
            buffer.Destroy(allocator);
        }
    }
}
