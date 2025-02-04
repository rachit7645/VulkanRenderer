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

#ifndef RENDER_MANAGER_H
#define RENDER_MANAGER_H

#include <vulkan/vulkan.h>

#include "FreeCamera.h"
#include "RenderObject.h"
#include "IndirectBuffer.h"
#include "MeshBuffer.h"
#include "SceneBuffer.h"
#include "PostProcess/PostProcessPass.h"
#include "Forward/ForwardPass.h"
#include "Depth/DepthPass.h"
#include "ImGui/ImGuiPass.h"
#include "Vulkan/Context.h"
#include "Vulkan/MegaSet.h"
#include "Vulkan/FormatHelper.h"
#include "Util/Util.h"
#include "Util/FrameCounter.h"
#include "Engine/Window.h"
#include "Models/ModelManager.h"

namespace Renderer
{
    class RenderManager
    {
    public:
        RenderManager();
        ~RenderManager();

        // No copying
        RenderManager(const RenderManager&)            = delete;
        RenderManager& operator=(const RenderManager&) = delete;

        // Only moving
        RenderManager(RenderManager&& other)            = default;
        RenderManager& operator=(RenderManager&& other) = default;

        void Render();
        [[nodiscard]] bool HandleEvents();
    private:
        void WaitForFences();
        void AcquireSwapchainImage();
        void BeginFrame();
        void Update();
        void EndFrame();
        void SubmitQueue();
        void Resize();

        void InitImGui();
        void CreateSyncObjects();

        // Object handles
        Engine::Window m_window;
        Vk::Context    m_context;
        Vk::Swapchain  m_swapchain;

        Vk::FormatHelper m_formatHelper;

        Models::ModelManager m_modelManager;
        Vk::MegaSet          m_megaSet;

        // Render Passes
        PostProcess::PostProcessPass m_postProcessPass;
        Forward::ForwardPass         m_forwardPass;
        Depth::DepthPass             m_depthPass;
        DearImGui::ImGuiPass         m_imGuiPass;

        // Buffers
        MeshBuffer     m_meshBuffer;
        IndirectBuffer m_indirectBuffer;
        SceneBuffer    m_sceneBuffer;

        // Scene objects
        std::vector<Renderer::RenderObject> m_renderObjects;
        Renderer::FreeCamera                m_camera;

        // Sync objects
        std::array<VkFence, Vk::FRAMES_IN_FLIGHT> inFlightFences = {};

        // Frame index
        usize m_currentFIF = Vk::FRAMES_IN_FLIGHT - 1;
        // Frame counter
        Util::FrameCounter m_frameCounter = {};

        bool m_isSwapchainOk = true;

        DirLight m_sun =
        {
            .position  = {-30.0f, -30.0f, -10.0f, 1.0f},
            .color     = {1.0f,   0.956f, 0.898f, 1.0f},
            .intensity = {5.0f,   5.0f,   5.0f,   1.0f}
        };

        Util::DeletionQueue m_deletionQueue = {};
    };
}

#endif