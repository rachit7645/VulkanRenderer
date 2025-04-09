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
    struct LightsBufferGLSL
    {
        glm::vec2                   pointLightShadowPlanes                                       = {};
        u32                         dirLightCount                                                = 0;
        Objects::DirLight           dirLights[Objects::MAX_DIR_LIGHT_COUNT]                      = {};
        u32                         pointLightCount                                              = 0;
        Objects::PointLight         pointLights[Objects::MAX_POINT_LIGHT_COUNT]                  = {};
        u32                         shadowedPointLightCount                                      = 0;
        Objects::ShadowedPointLight shadowedPointLights[Objects::MAX_SHADOWED_POINT_LIGHT_COUNT] = {};
        u32                         spotLightCount                                               = 0;
        Objects::SpotLight          spotLights[Objects::MAX_SPOT_LIGHT_COUNT]                    = {};
        u32                         shadowedSpotLightCount                                       = 0;
        Objects::ShadowedSpotLight  shadowedSpotLights[Objects::MAX_SHADOWED_SPOT_LIGHT_COUNT]   = {};
    };

    LightsBuffer::LightsBuffer(VkDevice device, VmaAllocator allocator)
    {
        for (usize i = 0; i < buffers.size(); ++i)
        {
            buffers[i] = Vk::Buffer
            (
                allocator,
                sizeof(LightsBufferGLSL),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            const u32  count       = 0;
            const auto pMappedData = static_cast<u8*>(buffers[i].allocationInfo.pMappedData);

            std::memcpy(pMappedData + 0, &Objects::POINT_LIGHT_SHADOW_PLANES, sizeof(glm::vec2));

            std::memcpy(pMappedData + GetDirLightOffset(),           &count, sizeof(u32));
            std::memcpy(pMappedData + GetPointLightOffset(),         &count, sizeof(u32));
            std::memcpy(pMappedData + GetShadowedPointLightOffset(), &count, sizeof(u32));
            std::memcpy(pMappedData + GetSpotLightOffset(),          &count, sizeof(u32));
            std::memcpy(pMappedData + GetShadowedSpotLightOffset(),  &count, sizeof(u32));

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
        const std::span<const Objects::DirLight> inDirLights,
        const std::span<const Objects::PointLight> inPointLights,
        const std::span<const Objects::SpotLight> inSpotLights
    )
    {
        dirLights = WriteLights(FIF, GetDirLightOffset(), inDirLights, Objects::MAX_DIR_LIGHT_COUNT);

        {
            std::vector<Objects::ShadowedPointLight> inShadowedPointLights;
            inShadowedPointLights.resize(std::min<usize>(inPointLights.size(), Objects::MAX_SHADOWED_POINT_LIGHT_COUNT));

            for (usize i = 0; i < inShadowedPointLights.size(); ++i)
            {
                inShadowedPointLights[i] = Objects::ShadowedPointLight(inPointLights[i]);
            }

            const auto uploadedPointLights = (inPointLights.size() <= inShadowedPointLights.size()) ? std::span<Objects::PointLight>{} : std::span(inPointLights.begin() + inShadowedPointLights.size(), inPointLights.end());

            pointLights         = WriteLights(FIF, GetPointLightOffset(),         uploadedPointLights,               Objects::MAX_POINT_LIGHT_COUNT);
            shadowedPointLights = WriteLights(FIF, GetShadowedPointLightOffset(), std::span(inShadowedPointLights), Objects::MAX_SHADOWED_POINT_LIGHT_COUNT);
        }

        {
            std::vector<Objects::ShadowedSpotLight> inShadowedSpotLights;
            inShadowedSpotLights.resize(std::min<usize>(inSpotLights.size(), Objects::MAX_SHADOWED_SPOT_LIGHT_COUNT));

            for (usize i = 0; i < inShadowedSpotLights.size(); ++i)
            {
                inShadowedSpotLights[i] = Objects::ShadowedSpotLight(inSpotLights[i]);
            }

            const auto uploadedSpotLights = (inSpotLights.size() <= inShadowedSpotLights.size()) ? std::span<Objects::SpotLight>{} : std::span(inSpotLights.begin() + inShadowedSpotLights.size(), inSpotLights.end());

            spotLights         = WriteLights(FIF, GetSpotLightOffset(),         uploadedSpotLights,               Objects::MAX_SPOT_LIGHT_COUNT);
            shadowedSpotLights = WriteLights(FIF, GetShadowedSpotLightOffset(), std::span(inShadowedSpotLights), Objects::MAX_SHADOWED_SPOT_LIGHT_COUNT);
        }

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

    template <typename T>
    std::vector<std::remove_const_t<T>> LightsBuffer::WriteLights(usize FIF, VkDeviceSize offset, const std::span<T> lights, u32 maxLightCount)
    {
        const VkDeviceSize requiredSize   = lights.size_bytes();
        const VkDeviceSize maxAllowedSize = maxLightCount * sizeof(T);

        const VkDeviceSize size  = std::min(requiredSize, maxAllowedSize);
        const u32          count = size / sizeof(T);

        const auto pMappedData = static_cast<u8*>(buffers[FIF].allocationInfo.pMappedData) + offset;

        std::memcpy
        (
            pMappedData,
            &count,
            sizeof(u32)
        );

        if (size != 0 && !lights.empty())
        {
            std::memcpy
            (
                pMappedData + sizeof(u32),
                lights.data(),
                size
            );
        }

        return lights.empty() ? std::vector<std::remove_const_t<T>>{} : std::vector<std::remove_const_t<T>>(lights.begin(), lights.begin() + count);
    }

    VkDeviceSize LightsBuffer::GetDirLightOffset()
    {
        return offsetof(LightsBufferGLSL, dirLightCount);
    }

    VkDeviceSize LightsBuffer::GetPointLightOffset()
    {
        return offsetof(LightsBufferGLSL, pointLightCount);
    }

    VkDeviceSize LightsBuffer::GetShadowedPointLightOffset()
    {
        return offsetof(LightsBufferGLSL, shadowedPointLightCount);
    }

    VkDeviceSize LightsBuffer::GetSpotLightOffset()
    {
        return offsetof(LightsBufferGLSL, spotLightCount);
    }

    VkDeviceSize LightsBuffer::GetShadowedSpotLightOffset()
    {
        return offsetof(LightsBufferGLSL, shadowedSpotLightCount);
    }

    void LightsBuffer::Destroy(VmaAllocator allocator)
    {
        for (auto& buffer : buffers)
        {
            buffer.Destroy(allocator);
        }
    }
}
