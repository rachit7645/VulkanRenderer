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

        void WriteLights
        (
            usize FIF,
            VmaAllocator allocator,
            const std::span<const Objects::DirLight> inDirLights,
            const std::span<const Objects::PointLight> inPointLights,
            const std::span<const Objects::SpotLight> inSpotLights
        );

        [[nodiscard]] static VkDeviceSize GetDirLightOffset();
        [[nodiscard]] static VkDeviceSize GetPointLightOffset();
        [[nodiscard]] static VkDeviceSize GetShadowedPointLightOffset();
        [[nodiscard]] static VkDeviceSize GetSpotLightOffset();
        [[nodiscard]] static VkDeviceSize GetShadowedSpotLightOffset();

        void Destroy(VmaAllocator allocator);

        std::array<Vk::Buffer, Vk::FRAMES_IN_FLIGHT> buffers;

        std::vector<Objects::DirLight>           dirLights;
        std::vector<Objects::PointLight>         pointLights;
        std::vector<Objects::ShadowedPointLight> shadowedPointLights;
        std::vector<Objects::SpotLight>          spotLights;
        std::vector<Objects::ShadowedSpotLight>  shadowedSpotLights;
    private:
        template <typename T> requires Objects::IsLightType<T>
        [[nodiscard]] std::vector<T> WriteLights(usize FIF, const std::span<const T> lights);
    };
}

#endif
