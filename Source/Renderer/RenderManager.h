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

#include "Buffers/IndirectBuffer.h"
#include "Buffers/MeshBuffer.h"
#include "Buffers/SceneBuffer.h"
#include "PostProcess/RenderPass.h"
#include "Depth/RenderPass.h"
#include "ImGui/RenderPass.h"
#include "Skybox/RenderPass.h"
#include "Bloom/RenderPass.h"
#include "PointShadow/RenderPass.h"
#include "SpotShadow/RenderPass.h"
#include "GBuffer/RenderPass.h"
#include "Lighting/RenderPass.h"
#include "AO/VBGTAO/RenderPass.h"
#include "ShadowRT/RenderPass.h"
#include "TAA/RenderPass.h"
#include "Culling/Dispatch.h"
#include "IBL/Generator.h"
#include "Vulkan/Context.h"
#include "Vulkan/MegaSet.h"
#include "Vulkan/FormatHelper.h"
#include "Vulkan/FramebufferManager.h"
#include "Vulkan/AccelerationStructure.h"
#include "Vulkan/CommandBufferAllocator.h"
#include "Vulkan/Timeline.h"
#include "Util/Util.h"
#include "Util/FrameCounter.h"
#include "Engine/Window.h"
#include "Engine/Scene.h"
#include "Models/ModelManager.h"

namespace Renderer
{
    class RenderManager
    {
    public:
        explicit RenderManager(Engine::Config config);
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
        void WaitForTimeline();
        void AcquireSwapchainImage();
        void BeginFrame();
        void Update(const Vk::CommandBuffer& cmdBuffer);
        void ImGuiDisplay();
        void SubmitQueue();
        void EndFrame();
        void Resize();

        void InitImGui();

        Engine::Config m_config;

        // Frame index
        usize m_FIF        = 0;
        usize m_frameIndex = 0;

        // Frame counter
        Util::FrameCounter m_frameCounter = {};

        Util::DeletionQueue                                   m_globalDeletionQueue = {};
        std::array<Util::DeletionQueue, Vk::FRAMES_IN_FLIGHT> m_deletionQueues      = {};

        // Object handles
        Engine::Window             m_window;
        Vk::Context                m_context;
        Vk::CommandBufferAllocator m_cmdBufferAllocator;
        Vk::Swapchain              m_swapchain;
        Vk::Timeline               m_timeline;

        Vk::FormatHelper m_formatHelper;

        Vk::MegaSet               m_megaSet;
        Vk::FramebufferManager    m_framebufferManager;
        Vk::AccelerationStructure m_accelerationStructure;
        Models::ModelManager      m_modelManager;

        // Render Passes
        PostProcess::RenderPass m_postProcessPass;
        Depth::RenderPass       m_depthPass;
        DearImGui::RenderPass   m_imGuiPass;
        Skybox::RenderPass      m_skyboxPass;
        Bloom::RenderPass       m_bloomPass;
        PointShadow::RenderPass m_pointShadowPass;
        SpotShadow::RenderPass  m_spotShadowPass;
        GBuffer::RenderPass     m_gBufferPass;
        Lighting::RenderPass    m_lightingPass;
        AO::VBGTAO::RenderPass  m_vbgtaoPass;
        ShadowRT::RenderPass    m_shadowRTPass;
        TAA::RenderPass         m_taaPass;

        // Dispatches
        Culling::Dispatch m_cullingDispatch;

        // Generators
        IBL::Generator m_iblGenerator;

        // Buffers
        Buffers::MeshBuffer     m_meshBuffer;
        Buffers::IndirectBuffer m_indirectBuffer;
        Buffers::SceneBuffer    m_sceneBuffer;

        // Scene objects
        std::optional<Engine::Scene> m_scene = std::nullopt;

        bool m_isSwapchainOk = true;
    };
}

#endif