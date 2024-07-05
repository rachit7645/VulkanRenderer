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

#ifndef RENDER_MANAGER_H
#define RENDER_MANAGER_H

#include <memory>
#include <vulkan/vulkan.h>

#include "FreeCamera.h"
#include "RenderPasses/SwapchainPass.h"
#include "RenderPasses/ForwardPass.h"
#include "Vulkan/Context.h"
#include "Vulkan/VertexBuffer.h"
#include "Vulkan/Swapchain.h"
#include "Util/Util.h"
#include "Util/FrameCounter.h"
#include "Engine/Window.h"
#include "Models/Model.h"

namespace Renderer
{
    class RenderManager
    {
    public:
        explicit RenderManager(const std::shared_ptr<Engine::Window>& window);
        ~RenderManager();

        // No copying
        RenderManager(const RenderManager&)            = delete;
        RenderManager& operator=(const RenderManager&) = delete;

        // Only moving
        RenderManager(RenderManager&& other)            = default;
        RenderManager& operator=(RenderManager&& other) = default;

        void Render();
    private:
        void BeginFrame();
        void Update();
        void EndFrame();
        void SubmitQueue();
        void Reset();

        void InitImGui();
        void CreateSyncObjects();

        // Object handles
        std::shared_ptr<Engine::Window> m_window  = nullptr;
        std::shared_ptr<Vk::Context>    m_context = nullptr;

        // Render Passes
        RenderPasses::SwapchainPass m_swapPass;
        RenderPasses::ForwardPass   m_forwardPass;

        // Scene objects
        Models::Model        m_model;
        Renderer::FreeCamera m_camera = {};

        // Sync objects
        std::array<VkFence, Vk::FRAMES_IN_FLIGHT> inFlightFences = {};

        // Frame index
        usize m_currentFIF = 0;
        // Frame counter
        Util::FrameCounter m_frameCounter = {};
    };
}

#endif