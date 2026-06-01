/*
 * Copyright (c) 2023 - 2026 Rachit
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
#include "Buffers/TileLightIndexBuffer.h"
#include "Buffers/ExposureBuffers.h"
#include "Objects/Samplers.h"
#include "Tonemap/RenderPass.h"
#include "Depth/RenderPass.h"
#include "ImGui/RenderPass.h"
#include "Skybox/RenderPass.h"
#include "Bloom/RenderPass.h"
#include "PointShadow/RenderPass.h"
#include "GBuffer/RenderPass.h"
#include "Lighting/RenderPass.h"
#include "AO/VBAO/Dispatch.h"
#include "ShadowRT/RayDispatch.h"
#include "TAA/RenderPass.h"
#include "Culling/Dispatch.h"
#include "Engine/Cache.h"
#include "IBL/Generator.h"
#include "SpotShadow/RenderPass.h"
#include "TiledLighting/Dispatch.h"
#include "Exposure/Dispatch.h"
#include "Vulkan/Context.h"
#include "Vulkan/MegaSet.h"
#include "Vulkan/FormatHelper.h"
#include "Vulkan/FramebufferManager.h"
#include "Vulkan/AccelerationStructure.h"
#include "Vulkan/CommandBufferAllocator.h"
#include "Vulkan/GraphicsTimeline.h"
#include "Vulkan/ComputeTimeline.h"
#include "Vulkan/PipelineManager.h"
#include "Vulkan/StagingPool.h"
#include "Util/Types.h"
#include "Util/FrameCounter.h"
#include "Engine/Window.h"
#include "Engine/Scene.h"
#include "Models/ModelManager.h"
#include "Renderer/RenderConfig.h"
#include "Renderer/Debug/RenderPass.h"
#include "Util/Log.h"

#ifdef ENGINE_DLSS
#include "Renderer/DLSS/Evaluation.h"
#endif

namespace Renderer
{
    class RenderManager
    {
    public:
        RenderManager();
        ~RenderManager();

        RenderManager(const RenderManager&) noexcept = delete;
        RenderManager& operator=(const RenderManager&) noexcept = delete;

        RenderManager(RenderManager&& other) noexcept = delete;
        RenderManager& operator=(RenderManager&& other) noexcept = delete;

        void Render();
        [[nodiscard]] bool HandleEvents();
    private:
        void WaitForTimeline();
        void AcquireSwapchainImage();
        void BeginFrame();

        void RenderGraphicsQueueOnly();
        void RenderMultiQueue();

        void GBufferGeneration(const Vk::CommandBuffer& cmdBuffer);

        void Occlusion
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Buffers::SceneBuffer::Buffers& sceneBuffers,
            const std::string_view sceneDepthID,
            const std::string_view gNormalID
        );

        void TraceRays(const Vk::CommandBuffer& cmdBuffer);
        void Lighting(const Vk::CommandBuffer& cmdBuffer);
        void AntiAliasing(const Vk::CommandBuffer& cmdBuffer);
        void BlitToSwapchain(const Vk::CommandBuffer& cmdBuffer);

        void Update(const Vk::CommandBuffer& cmdBuffer);
        void ImGuiDisplay();

        void EndFrame();

        void Resize();

        void InitImGui();

        tf::Executor m_executor;

        Engine::Config m_config;

        usize m_FIF        = 0;
        usize m_frameIndex = 0;

        Util::FrameCounter m_frameCounter = {};

        Util::DeletionQueue                                   m_globalDeletionQueue = {};
        std::array<Util::DeletionQueue, Vk::FRAMES_IN_FLIGHT> m_deletionQueues      = {};

        Engine::Window m_window;
        Vk::Context    m_context;

        Renderer::RenderConfig m_renderConfig;

        Vk::CommandBufferAllocator                m_graphicsCmdBufferAllocator;
        std::optional<Vk::CommandBufferAllocator> m_computeCmdBufferAllocator = std::nullopt;

        Vk::Swapchain m_swapchain;

        Vk::GraphicsTimeline               m_graphicsTimeline;
        std::optional<Vk::ComputeTimeline> m_computeTimeline = std::nullopt;

        Vk::FormatHelper m_formatHelper;

        Vk::MegaSet            m_megaSet;
        Vk::StagingPool        m_stagingPool;
        Vk::FramebufferManager m_framebufferManager;
        Models::ModelManager   m_modelManager;
        Vk::PipelineManager    m_pipelineManager;

        std::optional<Vk::AccelerationStructure> m_accelerationStructure;

        Objects::Samplers m_samplers;

        ToneMap::RenderPass     m_toneMap;
        Depth::RenderPass       m_depth;
        DearImGui::RenderPass   m_imGui;
        Skybox::RenderPass      m_skybox;
        Bloom::RenderPass       m_bloom;
        PointShadow::RenderPass m_pointShadow;
        GBuffer::RenderPass     m_gBuffer;
        Lighting::RenderPass    m_lighting;
        ShadowRT::RayDispatch   m_shadowRT;
        TAA::RenderPass         m_taa;
        SpotShadow::RenderPass  m_spotShadow;
        Debug::RenderPass       m_debug;

        Culling::Dispatch       m_culling;
        AO::VBAO::Dispatch      m_vbao;
        TiledLighting::Dispatch m_tiledLighting;
        Exposure::Dispatch      m_exposure;

        #ifdef ENGINE_DLSS
        DLSS::Evaluation m_DLSS;
        #endif

        IBL::Generator m_iblGenerator;

        Buffers::MeshBuffer           m_meshBuffer;
        Buffers::IndirectBuffer       m_indirectBuffer;
        Buffers::TileLightIndexBuffer m_tiledLightIndexBuffer;
        Buffers::ExposureBuffers      m_exposureBuffer;

        Buffers::SceneBuffer m_sceneBuffer;

        std::optional<Engine::Scene> m_scene = std::nullopt;

        bool m_isSwapchainOk = true;
    };
}

#endif