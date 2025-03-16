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

#ifndef XE_GTAO_RENDER_PASS_H
#define XE_GTAO_RENDER_PASS_H

#include <Renderer/Scene.h>

#include "DepthPreFilter/Pipeline.h"
#include "Vulkan/CommandBuffer.h"
#include "Vulkan/Constants.h"
#include "Vulkan/FramebufferManager.h"
#include "Vulkan/MegaSet.h"
#include "Vulkan/TextureManager.h"
#include "Renderer/Scene.h"

namespace Renderer::AO::XeGTAO
{
    class RenderPass
    {
    public:
        RenderPass
        (
            const Vk::Context& context,
            Vk::FramebufferManager& framebufferManager,
            Vk::MegaSet& megaSet,
            Vk::TextureManager& textureManager
        );

        void Render
        (
            usize FIF,
            usize frameIndex,
            const Renderer::Scene& scene,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet
        );

        void Destroy(VkDevice device, VkCommandPool cmdPool);

        DepthPreFilter::Pipeline depthPreFilterPipeline;

        std::array<Vk::CommandBuffer, Vk::FRAMES_IN_FLIGHT> cmdBuffers;
    private:
        void PreFilterDepth
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Renderer::Scene& scene,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet
        );

        f32 m_effectRadius       = 0.5f;
        f32 m_effectFalloffRange = 0.615f;
        f32 m_radiusMultiplier   = 1.457f;
    };
}

#endif
