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

#ifndef POST_PROCESS_PASS_H
#define POST_PROCESS_PASS_H

#include "Pipeline.h"
#include "Vulkan/CommandBuffer.h"
#include "Vulkan/FramebufferManager.h"
#include "Renderer/Objects/Camera.h"
#include "Renderer/Objects/GlobalSamplers.h"

namespace Renderer::PostProcess
{
    class RenderPass
    {
    public:
        RenderPass
        (
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            const Vk::MegaSet& megaSet,
            Vk::FramebufferManager& framebufferManager
        );

        void Destroy(VkDevice device);

        void Render
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet,
            const Vk::TextureManager& textureManager,
            const Objects::Camera& camera,
            const Objects::GlobalSamplers& samplers
        );
    private:
        PostProcess::Pipeline m_pipeline;

        f32 m_bloomStrength = 0.031f;
    };
}

#endif