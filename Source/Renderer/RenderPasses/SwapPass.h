/*
 *    Copyright 2023 Rachit Khandelwal
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef SWAP_PASS_H
#define SWAP_PASS_H

#include "Vulkan/Framebuffer.h"
#include "Vulkan/DepthBuffer.h"
#include "Vulkan/RenderPass.h"
#include "Vulkan/CommandBuffer.h"
#include "Renderer/Pipelines/SwapPipeline.h"

namespace Renderer::RenderPasses
{
    class SwapPass
    {
    public:
        // Create swapchain pass
        SwapPass(const std::shared_ptr<Engine::Window>& window, const std::shared_ptr<Vk::Context>& context);
        // Recreate swapchain pass
        void Recreate(const std::shared_ptr<Engine::Window>& window, const std::shared_ptr<Vk::Context>& context);
        // Destroy swapchain pass
        void Destroy(VkDevice device, VmaAllocator allocator);

        // Render to swapchain
        void Render(usize FIF);
        // Present
        void Present(const std::shared_ptr<Vk::Context>& context, usize FIF);

        // Swapchain
        Vk::Swapchain swapchain;
        // Presentation render pass
        Vk::RenderPass renderPass;
        // Pipeline
        Pipelines::SwapPipeline pipeline;
        // Swap chain framebuffers
        std::vector<Vk::Framebuffer> framebuffers;
        // Command buffers
        std::array<Vk::CommandBuffer, Vk::FRAMES_IN_FLIGHT> cmdBuffers = {};
    private:
        // Initialise swapchain pass data
        void InitData(const std::shared_ptr<Vk::Context>& context);
        // Create renderpass
        void CreateRenderPass(VkDevice device);
        // Create framebuffers
        void CreateFramebuffers(VkDevice device);
        // Destroy data
        void DestroyData(VkDevice device, VmaAllocator allocator);
        // Create command buffers
        void CreateCmdBuffers(const std::shared_ptr<Vk::Context>& context);
    };
}

#endif