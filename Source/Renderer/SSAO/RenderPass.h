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

#ifndef SSAO_PASS_H
#define SSAO_PASS_H

#include <Renderer/SSAO/Blur/Pipeline.h>

#include "SampleBuffer.h"
#include "Occlusion/Pipeline.h"
#include "Blur/Pipeline.h"
#include "Vulkan/Constants.h"
#include "Vulkan/FramebufferManager.h"
#include "Renderer/Buffers/SceneBuffer.h"

namespace Renderer::SSAO
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
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet,
            const Buffers::SceneBuffer& sceneBuffer
        );

        void Destroy(VkDevice device, VmaAllocator allocator, VkCommandPool cmdPool);

        Occlusion::Pipeline occlusionPipeline;
        Blur::Pipeline      blurPipeline;

        std::array<Vk::CommandBuffer, Vk::FRAMES_IN_FLIGHT> cmdBuffers;

        SSAO::SampleBuffer sampleBuffer;
        u32                noiseTexture;
    private:
        void RenderOcclusion
        (
            usize FIF,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet,
            const Buffers::SceneBuffer& sceneBuffer
        );

        void RenderBlur
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet
        );

        f32 m_radius = 0.7f;
        f32 m_bias   = 0.001f;
        f32 m_power  = 1.5f;
    };
}

#endif
