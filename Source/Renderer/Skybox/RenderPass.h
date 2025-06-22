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
#include "Renderer/Objects/GlobalSamplers.h"

namespace Renderer::Skybox
{
    class RenderPass
    {
    public:
        RenderPass
        (
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            const Vk::MegaSet& megaSet
        );

        void Destroy(VkDevice device);

        void Render
        (
            usize FIF,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet,
            const Models::ModelManager& modelManager,
            const Buffers::SceneBuffer& sceneBuffer,
            const Objects::GlobalSamplers& samplers,
            const IBL::IBLMaps& iblMaps
        );
    private:
        Skybox::Pipeline m_pipeline;
    };
}

#endif
