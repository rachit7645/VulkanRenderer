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

#ifndef SKYBOX_PASS_H
#define SKYBOX_PASS_H

#include "Pipeline.h"
#include "Vulkan/Constants.h"
#include "Vulkan/GeometryBuffer.h"
#include "Vulkan/FramebufferManager.h"
#include "Renderer/Buffers/SceneBuffer.h"
#include "Renderer/IBL/IBLMaps.h"

namespace Renderer::Skybox
{
    class RenderPass
    {
    public:
        RenderPass
        (
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            Vk::MegaSet& megaSet,
            Vk::TextureManager& textureManager
        );

        void Destroy(VkDevice device, VkCommandPool cmdPool);

        void Render
        (
            usize FIF,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::GeometryBuffer& geometryBuffer,
            const Buffers::SceneBuffer& sceneBuffer,
            const IBL::IBLMaps& iblMaps,
            const Vk::TextureManager& textureManager,
            const Vk::MegaSet& megaSet
        );

        Skybox::Pipeline pipeline;

        std::array<Vk::CommandBuffer, Vk::FRAMES_IN_FLIGHT> cmdBuffers;
    };
}

#endif
