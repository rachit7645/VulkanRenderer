/*
 *    Copyright 2023 - 2024 Rachit Khandelwal
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

#include "Vulkan/DepthBuffer.h"
#include "Vulkan/CommandBuffer.h"
#include "Renderer/Pipelines/SwapPipeline.h"

namespace Renderer::RenderPasses
{
    class SwapchainPass
    {
    public:
        SwapchainPass(const std::shared_ptr<Engine::Window>& window, const std::shared_ptr<Vk::Context>& context);
        void Recreate(const std::shared_ptr<Engine::Window>& window, const std::shared_ptr<Vk::Context>& context);
        void Destroy(const std::shared_ptr<Vk::Context>& context);

        void Render(Vk::DescriptorCache& descriptorCache, usize FIF);
        void Present(VkQueue queue, usize FIF);

        Vk::Swapchain           swapchain;
        Pipelines::SwapPipeline pipeline;

        std::array<Vk::CommandBuffer, Vk::FRAMES_IN_FLIGHT> cmdBuffers = {};
    private:
        void CreateCmdBuffers(const std::shared_ptr<Vk::Context>& context);
    };
}

#endif