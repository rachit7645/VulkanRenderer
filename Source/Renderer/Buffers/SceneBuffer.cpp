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

#include "SceneBuffer.h"

#include "Renderer/Util/Jitter.h"
#include "Renderer/RenderConstants.h"
#include "Vulkan/DebugUtils.h"
#include "Util/Log.h"
#include "Util/Maths.h"

namespace Renderer::Buffers
{
    SceneBuffer::SceneBuffer
    (
        VkDevice device,
        VmaAllocator allocator,
        const Renderer::RenderConfig& renderConfig
    )
        : graphicsBuffers{device, allocator}
    {
        if (renderConfig.multiQueue.isSupported)
        {
            computeBuffers = SceneBuffer::Buffers(device, allocator);
        }

        for (usize i = 0; i < Vk::FRAMES_IN_FLIGHT; ++i)
        {
            Vk::SetDebugName(device, graphicsBuffers.sceneBuffers[i].handle, fmt::format("Graphics/SceneBuffer/{}",             i));
            Vk::SetDebugName(device, graphicsBuffers.lightBuffers[i].handle, fmt::format("Graphics/SceneBuffer/LightBuffer/{}", i));

            if (computeBuffers.has_value())
            {
                Vk::SetDebugName(device, computeBuffers->sceneBuffers[i].handle, fmt::format("Compute/SceneBuffer/{}",             i));
                Vk::SetDebugName(device, computeBuffers->lightBuffers[i].handle, fmt::format("Compute/SceneBuffer/LightBuffer/{}", i));
            }
        }
    }

    void SceneBuffer::Write
    (
        usize FIF,
        usize frameIndex,
        VmaAllocator allocator,
        VkExtent2D renderExtent,
        VkExtent2D displayExtent,
        const Engine::Scene& scene,
        const Renderer::RenderConfig& renderConfig,
        const Models::ModelManager& modelManager
    )
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Debug"))
            {
                ImGui::Checkbox("Main View: Pause Culling", &m_pauseCulling);

                ImGui::Separator();

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        auto builtPointLightList = scene.pointLights;
        auto builtSpotLightList  = scene.spotLights;

        for (const auto& renderObject : scene.renderObjects)
        {
            const auto& model = modelManager.GetModel(renderObject.modelID);

            const auto globalTransform = Maths::TransformMatrix
            (
               renderObject.position,
               renderObject.rotation,
               renderObject.scale
            );

            for (const auto& light : model.pointLights)
            {
                builtPointLightList.emplace_back(light.Transform(globalTransform));
            }

            for (const auto& light : model.spotLights)
            {
                builtSpotLightList.emplace_back(light.Transform(globalTransform));
            }
        }

        const usize shadowedPointLightCount = std::min<usize>(builtPointLightList.size(), GPU::MAX_SHADOWED_POINT_LIGHT_COUNT);
        const usize shadowedSpotLightCount  = std::min<usize>(builtSpotLightList.size(),  GPU::MAX_SHADOWED_SPOT_LIGHT_COUNT);

        shadowedPointLights.resize(shadowedPointLightCount);
        shadowedSpotLights.resize(shadowedSpotLightCount);

        for (usize i = 0; i < shadowedPointLightCount; ++i)
        {
            shadowedPointLights[i] = GPU::ShadowedPointLight(builtPointLightList[i]);
        }

        for (usize i = 0; i < shadowedSpotLightCount; ++i)
        {
            shadowedSpotLights[i] = GPU::ShadowedSpotLight(builtSpotLightList[i]);
        }

        const auto uploadedPointLights = std::span(builtPointLightList).subspan(shadowedPointLightCount);
        const auto uploadedSpotLights  = std::span(builtSpotLightList).subspan(shadowedSpotLightCount);

        pointLights = std::ranges::to<std::vector>(uploadedPointLights);
        spotLights  = std::ranges::to<std::vector>(uploadedSpotLights);

        const auto projection = Maths::InfiniteProjectionReverseZ
        (
            scene.camera.FOV,
            static_cast<f32>(displayExtent.width) /
            static_cast<f32>(displayExtent.height),
            Renderer::NEAR_PLANE
        );

        const auto jitter = Renderer::GetJitter
        (
            frameIndex,
            renderConfig.antiAliasingMode,
            renderExtent,
            displayExtent
        );

        auto jitteredProjection = projection;

        jitteredProjection[2][0] += jitter.x;
        jitteredProjection[2][1] += jitter.y;

        const auto view = scene.camera.GetViewMatrix();

        const auto projectionView         = projection         * view;
        const auto jitteredProjectionView = jitteredProjection * view;

        const auto previousMatrices = matrices;

        matrices = GPU::SceneMatrices
        {
            .projection                    = projection,
            .inverseProjection             = glm::inverse(projection),
            .jitteredProjection            = jitteredProjection,
            .inverseJitteredProjection     = glm::inverse(jitteredProjection),
            .view                          = view,
            .inverseView                   = glm::inverse(view),
            .projectionView                = projectionView,
            .inverseProjectionView         = glm::inverse(projectionView),
            .jitteredProjectionView        = jitteredProjectionView,
            .inverseJitteredProjectionView = glm::inverse(jitteredProjectionView)
        };

        if (!m_pauseCulling)
        {
            cullingJitteredProjectionView = matrices.jitteredProjectionView;
        }

        WriteScene
        (
            FIF,
            frameIndex,
            allocator,
            scene,
            previousMatrices,
            graphicsBuffers
        );

        if (renderConfig.multiQueue.isEnabled)
        {
            WriteScene
            (
                FIF,
                frameIndex,
                allocator,
                scene,
                previousMatrices,
                *computeBuffers
            );
        }
    }

    void SceneBuffer::WriteScene
    (
        usize FIF,
        usize frameIndex,
        VmaAllocator allocator,
        const Engine::Scene& scene,
        const GPU::SceneMatrices& previousMatrices,
        const SceneBuffer::Buffers& buffers
    )
    {
        const auto& lightsBuffer = buffers.lightBuffers[FIF];

        const std::span<const GPU::PointLight>         uploadedPointLights         = pointLights;
        const std::span<const GPU::ShadowedPointLight> uploadedShadowedPointLights = shadowedPointLights;
        const std::span<const GPU::SpotLight>          uploadedSpotLights          = spotLights;
        const std::span<const GPU::ShadowedSpotLight>  uploadedShadowedSpotLights  = shadowedSpotLights;

        std::memcpy
        (
            static_cast<u8*>(lightsBuffer.hostAddress) + GPU::LightsBuffer::GetSunOffset(),
            &scene.sun,
            sizeof(GPU::DirLight)
        );

        WriteLights(lightsBuffer, uploadedPointLights);
        WriteLights(lightsBuffer, uploadedShadowedPointLights);
        WriteLights(lightsBuffer, uploadedSpotLights);
        WriteLights(lightsBuffer, uploadedShadowedSpotLights);

        if (!(lightsBuffer.memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            Vk::CheckResult(vmaFlushAllocation(
                allocator,
                lightsBuffer.allocation,
                0,
                lightsBuffer.size),
                "Failed to flush allocation!"
            );
        }

        const GPU::SceneBuffer sceneBuffer =
        {
            .currentMatrices     = matrices,
            .previousMatrices    = previousMatrices,
            .cameraPosition      = scene.camera.position,
            .nearPlane           = Renderer::NEAR_PLANE,
            .FIF                 = static_cast<u32>(FIF),
            .frameIndex          = frameIndex,
            .Sun                 = lightsBuffer.deviceAddress + GPU::LightsBuffer::GetSunOffset(),
            .PointLights         = lightsBuffer.deviceAddress + GPU::LightsBuffer::GetPointLightOffset(),
            .ShadowedPointLights = lightsBuffer.deviceAddress + GPU::LightsBuffer::GetShadowedPointLightOffset(),
            .SpotLights          = lightsBuffer.deviceAddress + GPU::LightsBuffer::GetSpotLightOffset(),
            .ShadowedSpotLights  = lightsBuffer.deviceAddress + GPU::LightsBuffer::GetShadowedSpotLightOffset()
        };

        std::memcpy
        (
            buffers.sceneBuffers[FIF].hostAddress,
            &sceneBuffer,
            sizeof(GPU::SceneBuffer)
        );

        if (!(buffers.sceneBuffers[FIF].memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            Vk::CheckResult(vmaFlushAllocation(
                allocator,
                buffers.sceneBuffers[FIF].allocation,
                0,
                sizeof(GPU::SceneBuffer)),
                "Failed to flush allocation!"
            );
        }
    }

    template <typename T> requires GPU::IsLightType<T>
    void SceneBuffer::WriteLights(const Vk::Buffer& buffer, const std::span<const T> lights)
    {
        VkDeviceSize offset        = 0;
        u32          maxLightCount = 0;

        if constexpr (std::is_same_v<T, GPU::PointLight>)
        {
            offset        = GPU::LightsBuffer::GetPointLightOffset();
            maxLightCount = GPU::MAX_POINT_LIGHT_COUNT;
        }
        else if constexpr (std::is_same_v<T, GPU::ShadowedPointLight>)
        {
            offset        = GPU::LightsBuffer::GetShadowedPointLightOffset();
            maxLightCount = GPU::MAX_SHADOWED_POINT_LIGHT_COUNT;
        }
        else if constexpr (std::is_same_v<T, GPU::SpotLight>)
        {
            offset        = GPU::LightsBuffer::GetSpotLightOffset();
            maxLightCount = GPU::MAX_SPOT_LIGHT_COUNT;
        }
        else if constexpr (std::is_same_v<T, GPU::ShadowedSpotLight>)
        {
            offset        = GPU::LightsBuffer::GetShadowedSpotLightOffset();
            maxLightCount = GPU::MAX_SHADOWED_SPOT_LIGHT_COUNT;
        }
        else
        {
            static_assert(false, "Invalid light type!");
        }

        const VkDeviceSize requiredSize   = lights.size_bytes();
        const VkDeviceSize maxAllowedSize = maxLightCount * sizeof(T);

        const VkDeviceSize size  = std::min(requiredSize, maxAllowedSize);
        const u32          count = size / sizeof(T);

        auto* pointer = static_cast<u8*>(buffer.hostAddress) + offset;

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
    }

    void SceneBuffer::Destroy(VmaAllocator allocator)
    {
        graphicsBuffers.Destroy(allocator);

        if (computeBuffers.has_value())
        {
            computeBuffers->Destroy(allocator);
        }
    }

    SceneBuffer::Buffers::Buffers(VkDevice device, VmaAllocator allocator)
    {
        for (auto& buffer : sceneBuffers)
        {
            buffer = Vk::Buffer
            (
                device,
                allocator,
                sizeof(GPU::SceneBuffer),
                0,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                VMA_MEMORY_USAGE_AUTO
            );
        }

        for (auto& lightBuffer : lightBuffers)
        {
            lightBuffer = Vk::Buffer
            (
                device,
                allocator,
                sizeof(GPU::LightsBuffer),
                0,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            constexpr u32 ZERO = 0;

            auto* pMappedData = static_cast<u8*>(lightBuffer.hostAddress);

            std::memcpy(pMappedData + GPU::LightsBuffer::GetPointLightOffset(),         &ZERO, sizeof(u32));
            std::memcpy(pMappedData + GPU::LightsBuffer::GetShadowedPointLightOffset(), &ZERO, sizeof(u32));
            std::memcpy(pMappedData + GPU::LightsBuffer::GetSpotLightOffset(),          &ZERO, sizeof(u32));

            if (!(lightBuffer.memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
            {
                Vk::CheckResult(vmaFlushAllocation(
                    allocator,
                    lightBuffer.allocation,
                    0,
                    lightBuffer.size),
                    "Failed to flush allocation!"
                );
            }
        }
    }

    void SceneBuffer::Buffers::Destroy(VmaAllocator allocator)
    {
        for (auto& buffer : sceneBuffers)
        {
            buffer.Destroy(allocator);
        }

        for (auto& buffer : lightBuffers)
        {
            buffer.Destroy(allocator);
        }
    }
}