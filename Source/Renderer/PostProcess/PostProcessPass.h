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

#ifndef POST_PROCESS_PASS_H
#define POST_PROCESS_PASS_H

#include "PostProcessPipeline.h"
#include "Vulkan/DepthBuffer.h"
#include "Vulkan/CommandBuffer.h"
#include "Vulkan/Swapchain.h"

namespace Renderer::PostProcess
{
    class PostProcessPass
    {
    public:
        PostProcessPass
        (
            const Vk::Context& context,
            const Vk::Swapchain& swapchain,
            Vk::MegaSet& megaSet,
            Vk::TextureManager& textureManager
        );

        void Destroy(VkDevice device, VkCommandPool cmdPool);

        void Render(const Vk::MegaSet& megaSet, Vk::Swapchain& swapchain, usize FIF);

        PostProcess::PostProcessPipeline pipeline;

        std::array<Vk::CommandBuffer, Vk::FRAMES_IN_FLIGHT> cmdBuffers;
    };
}

#endif