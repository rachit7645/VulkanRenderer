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
    class Dispatch
    {
    public:
        Dispatch
        (
            const Vk::Context& context,
            Vk::FramebufferManager& framebufferManager,
            Vk::MegaSet& megaSet,
            Vk::TextureManager& textureManager
        );

        void Execute
        (
            usize FIF,
            usize frameIndex,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::FramebufferManager& framebufferManager,
            const Buffers::SceneBuffer& sceneBuffer,
            const std::string_view sceneDepthID,
            const std::string_view gNormalID,
            const Vk::MegaSet& megaSet,
            const Vk::TextureManager& textureManager
        );

        void Destroy(VkDevice device);

        Vk::TextureID hilbertLUT = 0;
    private:
        void PreFilterDepth
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet,
            const Vk::TextureManager& textureManager,
            const std::string_view sceneDepthID
        );

        void Occlusion
        (
            usize FIF,
            usize frameIndex,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet,
            const Vk::TextureManager& textureManager,
            const Buffers::SceneBuffer& sceneBuffer,
            const std::string_view gNormalID
        );

        void Denoise
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet,
            const Vk::TextureManager& textureManager
        );

        DepthPreFilter::Pipeline m_depthPreFilterPipeline;
        Occlusion::Pipeline      m_occlusionPipeline;
        Denoise::Pipeline        m_denoisePipeline;

        f32 m_finalValuePower = 1.0f;
        f32 m_thickness       = 0.25f;
    };
}

#endif
