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

#ifndef SWAP_PASS_H
#define SWAP_PASS_H

#include "SwapchainPipeline.h"
#include "Vulkan/DepthBuffer.h"
#include "Vulkan/CommandBuffer.h"

namespace Renderer::Swapchain
{
    class SwapchainPass
    {
    public:
        SwapchainPass
        (
            const Vk::Context& context,
            const Vk::Swapchain& swapchain,
            Vk::MegaSet& megaSet,
            Vk::TextureManager& textureManager
        );

        void Recreate
        (
            const Vk::Context& context,
            const Vk::Swapchain& swapchain,
            Vk::MegaSet& megaSet,
            Vk::TextureManager& textureManager
        );

        void Destroy(VkDevice device, VkCommandPool cmdPool);

        void Render(const Vk::MegaSet& megaSet, Vk::Swapchain& swapchain, usize FIF);

        Swapchain::SwapchainPipeline pipeline;

        std::array<Vk::CommandBuffer, Vk::FRAMES_IN_FLIGHT> cmdBuffers = {};
    };
}

#endif