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

#ifndef TILED_LIGHTING_PASS_H
#define TILED_LIGHTING_PASS_H

#include "Renderer/Buffers/SceneBuffer.h"
#include "Renderer/Buffers/TileLightIndexBuffer.h"
#include "Renderer/Objects/GlobalSamplers.h"
#include "Vulkan/FramebufferManager.h"
#include "Vulkan/MegaSet.h"
#include "Vulkan/PipelineManager.h"
#include "Vulkan/TextureManager.h"

namespace Renderer::TiledLighting
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
            VkDevice device,
            VmaAllocator allocator,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet,
            const Vk::TextureManager& textureManager,
            const Buffers::SceneBuffer& sceneBuffer,
            const Objects::GlobalSamplers& samplers,
            Buffers::TileLightIndexBuffer& tileLightIndexBuffer,
            Util::DeletionQueue& deletionQueue
        );
    private:
        void Bounds
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet,
            const Vk::TextureManager& textureManager,
            const Objects::GlobalSamplers& samplers
        );

        void Culling
        (
            usize FIF,
            VkDevice device,
            VmaAllocator allocator,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::MegaSet& megaSet,
            const Vk::TextureManager& textureManager,
            const Buffers::SceneBuffer& sceneBuffer,
            const Objects::GlobalSamplers& samplers,
            Buffers::TileLightIndexBuffer& tileLightIndexBuffer,
            Util::DeletionQueue& deletionQueue
        );
    };
}

#endif