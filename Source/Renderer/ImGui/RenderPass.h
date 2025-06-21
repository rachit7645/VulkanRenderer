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

#include "Pipeline.h"
#include "Vulkan/Constants.h"
#include "Vulkan/Swapchain.h"

namespace Renderer::DearImGui
{
    class RenderPass
    {
    public:
        RenderPass
        (
            const Vk::Context& context,
            const Vk::Swapchain& swapchain,
            Vk::MegaSet& megaSet,
            Vk::TextureManager& textureManager
        );

        void Destroy(VkDevice device, VmaAllocator allocator);

        void Render
        (
            usize FIF,
            VkDevice device,
            VmaAllocator allocator,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::MegaSet& megaSet,
            const Vk::TextureManager& textureManager,
            const Vk::Swapchain& swapchain,
            Util::DeletionQueue& deletionQueue
        );
    private:
        void RenderGui
        (
            usize FIF,
            VkDevice device,
            VmaAllocator allocator,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::MegaSet& megaSet,
            const Vk::TextureManager& textureManager,
            const Vk::Swapchain& swapchain,
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

        DearImGui::Pipeline m_pipeline;

        std::array<Vk::Buffer, Vk::FRAMES_IN_FLIGHT> m_vertexBuffers;
        std::array<Vk::Buffer, Vk::FRAMES_IN_FLIGHT> m_indexBuffers;
    };
}

#endif
