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

#ifndef LIGHTS_BUFFER_H
#define LIGHTS_BUFFER_H

#include "Vulkan/Buffer.h"
#include "Vulkan/Constants.h"
#include "Renderer/Objects/Lights.h"

namespace Renderer::Buffers
{
    class LightsBuffer
    {
    public:
        LightsBuffer(VkDevice device, VmaAllocator allocator);

        void WriteDirLights(usize FIF, const std::span<const Objects::DirLight> lights);
        void WritePointLights(usize FIF, const std::span<const Objects::PointLight> lights);
        void WriteSpotLights(usize FIF, const std::span<const Objects::SpotLight> lights);

        void Destroy(VmaAllocator allocator);

        std::array<Vk::Buffer, Vk::FRAMES_IN_FLIGHT> dirLightBuffers;
        std::array<Vk::Buffer, Vk::FRAMES_IN_FLIGHT> pointLightBuffers;
        std::array<Vk::Buffer, Vk::FRAMES_IN_FLIGHT> spotLightBuffers;
    private:
        template <typename LightType>
        void WriteLights(std::span<const LightType> lights, const Vk::Buffer& buffer);
    };
}

#endif
