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

#include "DepthPreFilter/Pipeline.h"
#include "Occlusion/Pipeline.h"
#include "Denoise/Pipeline.h"
#include "Vulkan/CommandBuffer.h"
#include "Vulkan/Constants.h"
#include "Vulkan/FramebufferManager.h"
#include "Vulkan/MegaSet.h"
#include "Vulkan/TextureManager.h"
#include "Renderer/Buffers/SceneBuffer.h"

namespace Renderer::AO::XeGTAO
{
    class RenderPass
    {
    public:
        RenderPass
        (
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            Vk::FramebufferManager& framebufferManager,
            Vk::MegaSet& megaSet,
            Vk::TextureManager& textureManager
        );

        void Render
        (
            usize FIF,
            usize frameIndex,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet,
            const Buffers::SceneBuffer& sceneBuffer
        );

        void Destroy(VkDevice device, VkCommandPool cmdPool);

        DepthPreFilter::Pipeline depthPreFilterPipeline;
        Occlusion::Pipeline      occlusionPipeline;
        Denoise::Pipeline        denoisePipeline;

        std::array<Vk::CommandBuffer, Vk::FRAMES_IN_FLIGHT> cmdBuffers;

        u32 hilbertLUT = 0;
    private:
        void PreFilterDepth
        (
            usize frameIndex,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet
        );

        void Occlusion
        (
            usize FIF,
            usize frameIndex,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet,
            const Buffers::SceneBuffer& sceneBuffer
        );

        void Denoise
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet
        );

        f32 m_finalValuePower = 1.7f;
    };
}

#endif
