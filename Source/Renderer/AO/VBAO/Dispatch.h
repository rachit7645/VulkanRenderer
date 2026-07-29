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

#ifndef VBAO_DISPATCH_H
#define VBAO_DISPATCH_H

#include "Vulkan/Context.h"
#include "Renderer/Buffers/SceneBuffer.h"
#include "Renderer/Objects/Samplers.h"
#include "Vulkan/FramebufferManager.h"
#include "Vulkan/PipelineManager.h"
#include "Vulkan/TextureManager.h"

namespace Renderer::AO::VBAO
{
    class Dispatch
    {
    public:
        Dispatch
        (
            const Vk::MegaSet& megaSet,
            Vk::PipelineManager& pipelineManager,
            Vk::FramebufferManager& framebufferManager
        );

        void Execute
        (
            usize FIF,
            usize frameIndex,
            const std::string_view sceneDepthID,
            const std::string_view gNormalID,
            const Vk::CommandBuffer& cmdBuffer,
            const Renderer::RenderConfig& renderConfig,
            const Vk::PipelineManager& pipelineManager,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet,
            const Buffers::SceneBuffer::Buffers& sceneBuffers,
            const Objects::Samplers& samplers,
            Vk::TextureManager& textureManager,
            Scratch::Allocator& scratchAllocator
        );

        Vk::TextureID hilbertLUT = {};
    private:
        void PreFilterDepth
        (
            const std::string_view sceneDepthID,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet,
            const Vk::TextureManager& textureManager,
            const Objects::Samplers& samplers
        );

        void Occlusion
        (
            usize FIF,
            usize frameIndex,
            const std::string_view gNormalID,
            const Vk::CommandBuffer& cmdBuffer,
            const Renderer::RenderConfig& renderConfig,
            const Vk::PipelineManager& pipelineManager,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet,
            const Buffers::SceneBuffer::Buffers& sceneBuffers,
            const Objects::Samplers& samplers,
            Vk::TextureManager& textureManager,
            Scratch::Allocator& scratchAllocator
        );

        void Denoise
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet,
            const Vk::TextureManager& textureManager,
            const Objects::Samplers& samplers
        );

        f32 m_finalValuePower = 1.5f;
        f32 m_thickness       = 0.25f;
    };
}

#endif
