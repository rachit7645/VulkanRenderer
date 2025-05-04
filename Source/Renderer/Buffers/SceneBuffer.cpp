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

#include "SceneBuffer.h"

#include "Renderer/RenderConstants.h"
#include "Vulkan/DebugUtils.h"
#include "Util/Log.h"
#include "Util/Maths.h"

namespace Renderer::Buffers
{
    SceneBuffer::SceneBuffer(VkDevice device, VmaAllocator allocator)
        : lightsBuffer(device, allocator)
    {
        for (usize i = 0; i < buffers.size(); ++i)
        {
            buffers[i] = Vk::Buffer
            (
                allocator,
                sizeof(Renderer::Scene),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            buffers[i].GetDeviceAddress(device);

            Vk::SetDebugName(device, buffers[i].handle, fmt::format("SceneBuffer/{}", i));
        }
    }

    void SceneBuffer::WriteScene
    (
        usize FIF,
        usize frameIndex,
        VmaAllocator allocator,
        VkExtent2D swapchainExtent,
        const Engine::Scene& scene
    )
    {
        lightsBuffer.WriteLights
        (
            FIF,
            allocator,
            {&scene.sun, 1},
            scene.pointLights,
            scene.spotLights
        );
        
        gpuScene.previousMatrices = gpuScene.currentMatrices;

        const auto projection = Maths::CreateInfiniteProjectionReverseZ
        (
            scene.camera.FOV,
            static_cast<f32>(swapchainExtent.width) /
            static_cast<f32>(swapchainExtent.height),
            Renderer::NEAR_PLANE
        );

        auto jitter = Renderer::JITTER_SAMPLES[frameIndex % JITTER_SAMPLE_COUNT];
        jitter     -= glm::vec2(0.5f);
        jitter     /= glm::vec2(swapchainExtent.width, swapchainExtent.height);

        auto jitteredProjection = projection;

        jitteredProjection[2][0] += jitter.x;
        jitteredProjection[2][1] += jitter.y;

        const auto view = scene.camera.GetViewMatrix();

        gpuScene.currentMatrices  =
        {
            .projection         = projection,
            .inverseProjection  = glm::inverse(jitteredProjection),
            .jitteredProjection = jitteredProjection,
            .view               = view,
            .inverseView        = glm::inverse(view),
            .normalView         = Maths::CreateNormalMatrix(view)
        };

        gpuScene.cameraPosition = scene.camera.position;
        gpuScene.nearPlane      = Renderer::NEAR_PLANE;
        gpuScene.farPlane       = Renderer::FAR_PLANE; // There isn't actually a far plane right now lol

        const auto lightsBufferAddress = lightsBuffer.buffers[FIF].deviceAddress;

        gpuScene.commonLight         = lightsBufferAddress + 0;
        gpuScene.dirLights           = lightsBufferAddress + lightsBuffer.GetDirLightOffset();
        gpuScene.pointLights         = lightsBufferAddress + lightsBuffer.GetPointLightOffset();
        gpuScene.shadowedPointLights = lightsBufferAddress + lightsBuffer.GetShadowedPointLightOffset();
        gpuScene.spotLights          = lightsBufferAddress + lightsBuffer.GetSpotLightOffset();
        gpuScene.shadowedSpotLights  = lightsBufferAddress + lightsBuffer.GetShadowedSpotLightOffset();
        
        std::memcpy
        (
            buffers[FIF].allocationInfo.pMappedData,
            &gpuScene,
            sizeof(Renderer::Scene)
        );

        if (!(buffers[FIF].memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            Vk::CheckResult(vmaFlushAllocation(
                allocator,
                buffers[FIF].allocation,
                0,
                sizeof(Renderer::Scene)),
                "Failed to flush allocation!"
            );
        }
    }

    void SceneBuffer::Destroy(VmaAllocator allocator)
    {
        lightsBuffer.Destroy(allocator);

        for (auto& buffer : buffers)
        {
            buffer.Destroy(allocator);
        }
    }
}
