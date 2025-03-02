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
#include "Depth/RenderPass.h"
#include "ImGui/RenderPass.h"
#include "Skybox/RenderPass.h"
#include "Bloom/RenderPass.h"
#include "Shadow/RenderPass.h"
#include "PointShadow/RenderPass.h"
#include "SpotShadow/RenderPass.h"
#include "GBuffer/RenderPass.h"
#include "Lighting/RenderPass.h"
#include "SSAO/RenderPass.h"
#include "Culling/Dispatch.h"
#include "Vulkan/Context.h"
#include "Vulkan/MegaSet.h"
#include "Vulkan/FormatHelper.h"
#include "Vulkan/FramebufferManager.h"
#include "Util/Util.h"
#include "Util/FrameCounter.h"
#include "Engine/Window.h"
#include "Models/ModelManager.h"
#include "Vulkan/AccelerationStructure.h"

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

        Vk::MegaSet               m_megaSet;
        Vk::FramebufferManager    m_framebufferManager;
        Vk::AccelerationStructure m_as;
        Models::ModelManager      m_modelManager;

        IBL::IBLMaps m_iblMaps;

        // Render Passes
        PostProcess::RenderPass m_postProcessPass;
        Depth::RenderPass       m_depthPass;
        DearImGui::RenderPass   m_imGuiPass;
        Skybox::RenderPass      m_skyboxPass;
        Bloom::RenderPass       m_bloomPass;
        Shadow::RenderPass      m_shadowPass;
        PointShadow::RenderPass m_pointShadowPass;
        SpotShadow::RenderPass  m_spotShadowPass;
        GBuffer::RenderPass     m_gBufferPass;
        Lighting::RenderPass    m_lightingPass;
        SSAO::RenderPass        m_ssaoPass;
        Culling::Dispatch       m_cullingDispatch;

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

        Scene m_scene = {};

        Objects::DirLight                  m_sun;
        std::array<Objects::PointLight, 2> m_pointLights;
        std::array<Objects::SpotLight,  2> m_spotLights;

        Util::DeletionQueue m_deletionQueue = {};
    };
}

#endif