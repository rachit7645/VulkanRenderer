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

#include "Util/Concept.h"
#include "Vulkan/DebugUtils.h"
#include "Util/Log.h"

namespace Renderer::Buffers
{
    struct GPULights
    {
        GPU::DirLight           sun                                                      = {};
        u32                     pointLightCount                                          = 0;
        GPU::PointLight         pointLights[GPU::MAX_POINT_LIGHT_COUNT]                  = {};
        u32                     shadowedPointLightCount                                  = 0;
        GPU::ShadowedPointLight shadowedPointLights[GPU::MAX_SHADOWED_POINT_LIGHT_COUNT] = {};
        u32                     spotLightCount                                           = 0;
        GPU::SpotLight          spotLights[GPU::MAX_SPOT_LIGHT_COUNT]                    = {};
        u32                     shadowedSpotLightCount                                   = 0;
        GPU::ShadowedSpotLight  shadowedSpotLights[GPU::MAX_SHADOWED_SPOT_LIGHT_COUNT]   = {};
    };

    LightsBuffer::LightsBuffer(VkDevice device, VmaAllocator allocator)
    {
        for (usize i = 0; i < buffers.size(); ++i)
        {
            buffers[i] = Vk::Buffer
            (
                allocator,
                sizeof(GPULights),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            constexpr u32 ZERO = 0;

            const auto pMappedData = static_cast<u8*>(buffers[i].allocationInfo.pMappedData);

            std::memcpy(pMappedData + GetPointLightOffset(),         &ZERO, sizeof(u32));
            std::memcpy(pMappedData + GetShadowedPointLightOffset(), &ZERO, sizeof(u32));
            std::memcpy(pMappedData + GetSpotLightOffset(),          &ZERO, sizeof(u32));

            if (!(buffers[i].memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
            {
                Vk::CheckResult(vmaFlushAllocation(
                    allocator,
                    buffers[i].allocation,
                    0,
                    buffers[i].size),
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
        const GPU::DirLight& inSun,
        const std::span<const GPU::PointLight> inPointLights,
        const std::span<const GPU::SpotLight> inSpotLights
    )
    {
        WriteSunLight(FIF, inSun);

        // Point Lights
        {
            std::vector<GPU::ShadowedPointLight> inShadowedPointLights;

            inShadowedPointLights.resize(std::min<usize>(inPointLights.size(), GPU::MAX_SHADOWED_POINT_LIGHT_COUNT));

            for (usize i = 0; i < inShadowedPointLights.size(); ++i)
            {
                inShadowedPointLights[i] = GPU::ShadowedPointLight(inPointLights[i]);
            }

            const auto uploadedPointLights = (inPointLights.size() <= inShadowedPointLights.size()) ?
                                             std::span<GPU::PointLight>{} :
                                             std::span(inPointLights.begin() + static_cast<ssize>(inShadowedPointLights.size()), inPointLights.end());

            const std::span<const GPU::ShadowedPointLight> uploadedShadowedPointLights = inShadowedPointLights;

            pointLights         = WriteLights(FIF, uploadedPointLights);
            shadowedPointLights = WriteLights(FIF, uploadedShadowedPointLights);
        }

        // Spot Lights
        {
            std::vector<GPU::ShadowedSpotLight> inShadowedSpotLights;

            inShadowedSpotLights.resize(std::min<usize>(inSpotLights.size(), GPU::MAX_SHADOWED_SPOT_LIGHT_COUNT));

            for (usize i = 0; i < inShadowedSpotLights.size(); ++i)
            {
                inShadowedSpotLights[i] = GPU::ShadowedSpotLight(inSpotLights[i]);
            }

            const auto uploadedSpotLights = (inSpotLights.size() <= inShadowedSpotLights.size()) ?
                                             std::span<GPU::SpotLight>{} :
                                             std::span(inSpotLights.begin() + static_cast<ssize>(inShadowedSpotLights.size()), inSpotLights.end());

            const std::span<const GPU::ShadowedSpotLight> uploadedShadowedSpotLights = inShadowedSpotLights;

            spotLights         = WriteLights(FIF, uploadedSpotLights);
            shadowedSpotLights = WriteLights(FIF, uploadedShadowedSpotLights);
        }

        if (!(buffers[FIF].memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            Vk::CheckResult(vmaFlushAllocation(
                allocator,
                buffers[FIF].allocation,
                0,
                buffers[FIF].size),
                "Failed to flush allocation!"
            );
        }
    }

    void LightsBuffer::WriteSunLight(usize FIF, const GPU::DirLight& inSun)
    {
        sun = inSun;

        const auto offset  = GetSunOffset();
        const auto pointer = static_cast<u8*>(buffers[FIF].allocationInfo.pMappedData) + offset;

        std::memcpy
        (
            pointer,
            &sun,
            sizeof(GPU::DirLight)
        );
    }

    template <typename T> requires GPU::IsLightType<T>
    std::vector<T> LightsBuffer::WriteLights(usize FIF, const std::span<const T> lights)
    {
        VkDeviceSize offset        = 0;
        u32          maxLightCount = 0;

        if constexpr (std::is_same_v<T, GPU::PointLight>)
        {
            offset        = GetPointLightOffset();
            maxLightCount = GPU::MAX_POINT_LIGHT_COUNT;
        }
        else if constexpr (std::is_same_v<T, GPU::ShadowedPointLight>)
        {
            offset        = GetShadowedPointLightOffset();
            maxLightCount = GPU::MAX_SHADOWED_POINT_LIGHT_COUNT;
        }
        else if constexpr (std::is_same_v<T, GPU::SpotLight>)
        {
            offset        = GetSpotLightOffset();
            maxLightCount = GPU::MAX_SPOT_LIGHT_COUNT;
        }
        else if constexpr (std::is_same_v<T, GPU::ShadowedSpotLight>)
        {
            offset        = GetShadowedSpotLightOffset();
            maxLightCount = GPU::MAX_SHADOWED_SPOT_LIGHT_COUNT;
        }
        else
        {
            static_assert(Util::AlwaysFalse<T>, "Invalid light type!");
        }

        const VkDeviceSize requiredSize   = lights.size_bytes();
        const VkDeviceSize maxAllowedSize = maxLightCount * sizeof(T);

        const VkDeviceSize size  = std::min(requiredSize, maxAllowedSize);
        const u32          count = size / sizeof(T);

        const auto pointer = static_cast<u8*>(buffers[FIF].allocationInfo.pMappedData) + offset;

        std::memcpy
        (
            pointer,
            &count,
            sizeof(u32)
        );

        if (size != 0 && !lights.empty())
        {
            std::memcpy
            (
                pointer + sizeof(u32),
                lights.data(),
                size
            );
        }

        return lights.empty() ?
               std::vector<T>{} :
               std::vector<T>(lights.begin(), lights.begin() + count);
    }

    VkDeviceSize LightsBuffer::GetSunOffset()
    {
        return offsetof(GPULights, sun);
    }

    VkDeviceSize LightsBuffer::GetPointLightOffset()
    {
        return offsetof(GPULights, pointLightCount);
    }

    VkDeviceSize LightsBuffer::GetShadowedPointLightOffset()
    {
        return offsetof(GPULights, shadowedPointLightCount);
    }

    VkDeviceSize LightsBuffer::GetSpotLightOffset()
    {
        return offsetof(GPULights, spotLightCount);
    }

    VkDeviceSize LightsBuffer::GetShadowedSpotLightOffset()
    {
        return offsetof(GPULights, shadowedSpotLightCount);
    }

    void LightsBuffer::Destroy(VmaAllocator allocator)
    {
        for (auto& buffer : buffers)
        {
            buffer.Destroy(allocator);
        }
    }
}
