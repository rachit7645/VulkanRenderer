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

#ifndef RENDER_MANAGER_H
#define RENDER_MANAGER_H

#include <memory>
#include <vulkan/vulkan.h>

#include "SwapPipeline.h"
#include "Models/Model.h"
#include "Vulkan/Context.h"
#include "Engine/Window.h"
#include "Util/Util.h"
#include "Vulkan/VertexBuffer.h"
#include "Vulkan/Swapchain.h"
#include "Util/FrameCounter.h"

namespace Renderer
{
    class RenderManager
    {
    public:
        // Constructor
        explicit RenderManager(std::shared_ptr<Engine::Window> window);
        // Destructor
        ~RenderManager();

        // Render frame
        void Render();
    private:
        // Begin frame
        void BeginFrame();
        // Wait for previous frame
        void WaitForFrame();
        // Acquire swap chain image
        void AcquireSwapChainImage();
        // Update
        void Update();
        // End frame
        void EndFrame();
        // Submit queue
        void SubmitQueue();
        // Present
        void Present();
        // Check if swap chain is valid
        bool IsSwapchainValid();

        // Pointer to window
        std::shared_ptr<Engine::Window> m_window = nullptr;
        // Vulkan context
        std::shared_ptr<Vk::Context> m_vkContext = nullptr;
        // Swap chain
        std::shared_ptr<Vk::Swapchain> m_swapchain = nullptr;
        // Render pipeline
        std::unique_ptr<Renderer::SwapPipeline> m_renderPipeline = nullptr;
        // Model
        std::unique_ptr<Models::Model> m_model;

        // Image index
        u32 m_imageIndex = 0;
        // Frame index
        usize m_currentFrame = 0;

        // Current command buffer
        VkCommandBuffer currentCmdBuffer = VK_NULL_HANDLE;
        // Status
        std::array<VkResult, 2> m_swapchainStatus = {VK_SUCCESS, VK_SUCCESS};

        // Frame counter
        Util::FrameCounter m_frameCounter = {};
    };
}

#endif