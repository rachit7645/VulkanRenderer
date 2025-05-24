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

#ifndef VBGTAO_RENDER_PASS_H
#define VBGTAO_RENDER_PASS_H

#include "DepthPreFilter/Pipeline.h"
#include "Occlusion/Pipeline.h"
#include "Denoise/Pipeline.h"
#include "Vulkan/Context.h"
#include "Vulkan/FramebufferManager.h"
#include "Vulkan/TextureManager.h"
#include "Models/ModelManager.h"
#include "Renderer/Buffers/SceneBuffer.h"

namespace Renderer::AO::VBGTAO
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
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            const Vk::FramebufferManager& framebufferManager,
            const Buffers::SceneBuffer& sceneBuffer,
            Vk::MegaSet& megaSet,
            Models::ModelManager& modelManager,
            Util::DeletionQueue& deletionQueue
        );

        void Destroy(VkDevice device);
    private:
        void PreFilterDepth
        (
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
            const Vk::TextureManager& textureManager,
            const Buffers::SceneBuffer& sceneBuffer
        );

        void Denoise
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet
        );

        DepthPreFilter::Pipeline m_depthPreFilterPipeline;
        Occlusion::Pipeline      m_occlusionPipeline;
        Denoise::Pipeline        m_denoisePipeline;

        std::optional<Vk::TextureID> m_hilbertLUT = std::nullopt;

        f32 m_finalValuePower = 1.3f;
        f32 m_thickness       = 0.25f;
    };
}

#endif
