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

#ifndef SCENE_BUFFER_H
#define SCENE_BUFFER_H

#include "Vulkan/Buffer.h"
#include "Vulkan/Constants.h"
#include "Engine/Scene.h"
#include "GPU/Scene.h"
#include "Renderer/RenderConfig.h"

namespace Renderer::Buffers
{
    class SceneBuffer
    {
    public:
        struct Buffers
        {
            Buffers(VkDevice device, VmaAllocator allocator);

            void Destroy(VmaAllocator allocator);

            std::array<Vk::Buffer, Vk::FRAMES_IN_FLIGHT> sceneBuffers = {};
            std::array<Vk::Buffer, Vk::FRAMES_IN_FLIGHT> lightBuffers = {};
        };

        SceneBuffer
        (
            VkDevice device,
            VmaAllocator allocator,
            const Renderer::RenderConfig& renderConfig
        );

        void Write
        (
            usize FIF,
            usize frameIndex,
            VmaAllocator allocator,
            VkExtent2D renderExtent,
            VkExtent2D displayExtent,
            const Engine::Scene& scene,
            const Renderer::RenderConfig& renderConfig,
            const Models::ModelManager& modelManager
        );

        void Destroy(VmaAllocator allocator);

        GPU::SceneMatrices matrices = {};

        glm::mat4 cullingJitteredProjectionView = {};

        std::vector<GPU::PointLight>         pointLights         = {};
        std::vector<GPU::ShadowedPointLight> shadowedPointLights = {};
        std::vector<GPU::SpotLight>          spotLights          = {};
        std::vector<GPU::ShadowedSpotLight>  shadowedSpotLights  = {};

        SceneBuffer::Buffers graphicsBuffers;

        std::optional<SceneBuffer::Buffers> computeBuffers = std::nullopt;
    private:
        void WriteScene
        (
            usize FIF,
            usize frameIndex,
            VmaAllocator allocator,
            const Engine::Scene& scene,
            const GPU::SceneMatrices& previousMatrices,
            const SceneBuffer::Buffers& buffers
        );

        template <typename T> requires GPU::IsLightType<T>
        void WriteLights(const Vk::Buffer& buffer, const std::span<const T> lights);

        bool m_pauseCulling = false;
    };
}

#endif
