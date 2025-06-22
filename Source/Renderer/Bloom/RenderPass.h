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

#ifndef BLOOM_PASS_H
#define BLOOM_PASS_H

#include "Vulkan/Constants.h"
#include "Vulkan/FramebufferManager.h"
#include "DownSample/Regular/Pipeline.h"
#include "DownSample/FirstSample/Pipeline.h"
#include "UpSample/Pipeline.h"
#include "Renderer/Objects/GlobalSamplers.h"

namespace Renderer::Bloom
{
    class RenderPass
    {
    public:
        RenderPass
        (
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            Vk::MegaSet& megaSet,
            Vk::FramebufferManager& framebufferManager
        );

        void Render
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet,
            const Vk::TextureManager& textureManager,
            const Objects::GlobalSamplers& samplers
        );

        void Destroy(VkDevice device);
    private:
        void RenderDownSamples
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet,
            const Vk::TextureManager& textureManager,
            const Objects::GlobalSamplers& samplers
        );

        void RenderUpSamples
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet,
            const Vk::TextureManager& textureManager,
            const Objects::GlobalSamplers& samplers
        );

        DownSample::FirstSample::Pipeline m_downSampleFirstSamplePipeline;
        DownSample::Regular::Pipeline     m_downsampleRegularPipeline;
        UpSample::Pipeline                m_upsamplePipeline;

        f32 m_filterRadius = 0.005f;
    };
}

#endif
