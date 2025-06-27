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

#ifndef IMGUI_PASS_H
#define IMGUI_PASS_H

#include "Models/ModelManager.h"
#include "Renderer/Objects/GlobalSamplers.h"
#include "Vulkan/Constants.h"
#include "Vulkan/PipelineManager.h"
#include "Vulkan/Swapchain.h"

namespace Renderer::DearImGui
{
    class RenderPass
    {
    public:
        RenderPass
        (
            const Vk::Swapchain& swapchain,
            const Vk::MegaSet& megaSet,
            Vk::PipelineManager& pipelineManager
        );

        void Render
        (
            usize FIF,
            VkDevice device,
            VmaAllocator allocator,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Vk::Swapchain& swapchain,
            const Objects::GlobalSamplers& samplers,
            Vk::MegaSet& megaSet,
            Models::ModelManager& modelManager,
            Util::DeletionQueue& deletionQueue
        );

        void Destroy(VmaAllocator allocator);
    private:
        void RenderGui
        (
            usize FIF,
            VkDevice device,
            VmaAllocator allocator,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Vk::Swapchain& swapchain,
            const Objects::GlobalSamplers& samplers,
            Vk::MegaSet& megaSet,
            Models::ModelManager& modelManager,
            Util::DeletionQueue& deletionQueue,
            const ImDrawData* drawData
        );

        void UploadToBuffers
        (
            usize FIF,
            VkDevice device,
            VmaAllocator allocator,
            const Vk::CommandBuffer& cmdBuffer,
            Vk::Buffer& vertexBuffer,
            Vk::Buffer& indexBuffer,
            Util::DeletionQueue& deletionQueue,
            const ImDrawData* drawData
        );

        void UpdateTextures
        (
            VkDevice device,
            VmaAllocator allocator,
            const Vk::CommandBuffer& cmdBuffer,
            Vk::MegaSet& megaSet,
            Models::ModelManager& modelManager,
            Util::DeletionQueue& deletionQueue,
            const ImDrawData* drawData
        );

        std::array<Vk::Buffer, Vk::FRAMES_IN_FLIGHT> m_vertexBuffers;
        std::array<Vk::Buffer, Vk::FRAMES_IN_FLIGHT> m_indexBuffers;
    };
}

#endif
