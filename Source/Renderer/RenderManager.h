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

#include "RenderObject.h"
#include "Objects/FreeCamera.h"
#include "Buffers/IndirectBuffer.h"
#include "Buffers/MeshBuffer.h"
#include "Buffers/SceneBuffer.h"
#include "Buffers/LightsBuffer.h"
#include "IBL/IBLMaps.h"
#include "PostProcess/RenderPass.h"
#include "Forward/RenderPass.h"
#include "Depth/RenderPass.h"
#include "ImGui/RenderPass.h"
#include "Skybox/RenderPass.h"
#include "Bloom/RenderPass.h"
#include "Shadow/RenderPass.h"
#include "Vulkan/Context.h"
#include "Vulkan/MegaSet.h"
#include "Vulkan/FormatHelper.h"
#include "Vulkan/FramebufferManager.h"
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
        RenderManager(RenderManager&& other)            noexcept = default;
        RenderManager& operator=(RenderManager&& other) noexcept = default;

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

        Vk::MegaSet            m_megaSet;
        Vk::FramebufferManager m_framebufferManager;
        Models::ModelManager   m_modelManager;

        IBL::IBLMaps m_iblMaps;

        // Render Passes
        PostProcess::RenderPass m_postProcessPass;
        Forward::RenderPass     m_forwardPass;
        Depth::RenderPass       m_depthPass;
        DearImGui::RenderPass   m_imGuiPass;
        Skybox::RenderPass      m_skyboxPass;
        Bloom::RenderPass       m_bloomPass;
        Shadow::RenderPass      m_shadowPass;

        // Buffers
        Buffers::MeshBuffer     m_meshBuffer;
        Buffers::IndirectBuffer m_indirectBuffer;
        Buffers::SceneBuffer    m_sceneBuffer;
        Buffers::LightsBuffer   m_lightsBuffer;

        // Scene objects
        std::vector<Renderer::RenderObject> m_renderObjects;
        Objects::FreeCamera                 m_camera;

        // Sync objects
        std::array<VkFence, Vk::FRAMES_IN_FLIGHT> inFlightFences = {};

        // Frame index
        usize m_currentFIF = Vk::FRAMES_IN_FLIGHT - 1;
        // Frame counter
        Util::FrameCounter m_frameCounter = {};

        bool m_isSwapchainOk = true;

        Objects::DirLight m_sun =
        {
            .position  = {-30.0f, -30.0f, -10.0f},
            .color     = {1.0f,   0.956f, 0.898f},
            .intensity = {2.5f,   2.5f,   2.5f}
        };


        std::array<Objects::PointLight, 2> m_pointLights =
        {
            Objects::PointLight
            {
                .position    = {4.0f, 20.0f,  -3.0f},
                .color       = {0.0f, 0.945f,  0.945f},
                .intensity   = {1.0f, 7.0f,    5.0f},
                .attenuation = {1.0f, 0.022f,  0.0019f}
            },
            Objects::PointLight
            {
                .position    = {27.0f, 15.0f, -3.0f},
                .color       = {0.0f,  0.031f, 1.0f},
                .intensity   = {1.0f,  6.0f,   10.0f},
                .attenuation = {1.0f,  0.027f, 0.0028f}
            }
        };

        std::array<Objects::SpotLight, 2> m_spotLights =
        {
            Objects::SpotLight
            {
                .position    = {22.0f, 10.0f,  6.0f},
                .color       = {1.0f,  0.0f,   0.0f},
                .intensity   = {10.0f, 1.0f,   1.0f},
                .attenuation = {1.0f,  0.007f, 0.0002f},
                .direction   = {-1.0f, 0.0f,  -0.3f},
                .cutOff      = {10.0f, 30.0f}
            },
            Objects::SpotLight
            {
                .position    = {62.0f,  2.0f,  -2.0f},
                .color       = {0.941f, 0.0f,   1.0f},
                .intensity   = {10.0f,  10.0f,  10.0f},
                .attenuation = {1.0f,   0.022f, 0.0019f},
                .direction   = {-1.0f,  0.3f,  -0.1f},
                .cutOff      = {10.0f,  50.0f}
            }
        };

        Util::DeletionQueue m_deletionQueue = {};
    };
}

#endif