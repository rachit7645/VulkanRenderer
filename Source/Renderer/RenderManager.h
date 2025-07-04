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
#include "Objects/GlobalSamplers.h"
#include "PostProcess/RenderPass.h"
#include "Depth/RenderPass.h"
#include "ImGui/RenderPass.h"
#include "Skybox/RenderPass.h"
#include "Bloom/RenderPass.h"
#include "PointShadow/RenderPass.h"
#include "GBuffer/RenderPass.h"
#include "Lighting/RenderPass.h"
#include "AO/VBGTAO/Dispatch.h"
#include "ShadowRT/RayDispatch.h"
#include "TAA/RenderPass.h"
#include "Culling/Dispatch.h"
#include "IBL/Generator.h"
#include "SpotShadow/RenderPass.h"
#include "Vulkan/Context.h"
#include "Vulkan/MegaSet.h"
#include "Vulkan/FormatHelper.h"
#include "Vulkan/FramebufferManager.h"
#include "Vulkan/AccelerationStructure.h"
#include "Vulkan/CommandBufferAllocator.h"
#include "Vulkan/GraphicsTimeline.h"
#include "Vulkan/ComputeTimeline.h"
#include "Vulkan/PipelineManager.h"
#include "Util/Types.h"
#include "Util/FrameCounter.h"
#include "Engine/Window.h"
#include "Engine/Scene.h"
#include "Models/ModelManager.h"

namespace Renderer
{
    class RenderManager
    {
    public:
        RenderManager();
        ~RenderManager();

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
            const Buffers::SceneBuffer& sceneBuffer,
            const std::string_view sceneDepthID,
            const std::string_view gNormalID
        );

        void TraceRays(const Vk::CommandBuffer& cmdBuffer);
        void Lighting(const Vk::CommandBuffer& cmdBuffer);
        void BlitToSwapchain(const Vk::CommandBuffer& cmdBuffer);

        void GraphicsToAsyncComputeRelease(const Vk::CommandBuffer& cmdBuffer);
        void GraphicsToAsyncComputeAcquire(const Vk::CommandBuffer& cmdBuffer);
        void AsyncComputeToGraphicsRelease(const Vk::CommandBuffer& cmdBuffer);
        void AsyncComputeToGraphicsAcquire(const Vk::CommandBuffer& cmdBuffer);

        void Update(const Vk::CommandBuffer& cmdBuffer);
        void ImGuiDisplay();

        void EndFrame();

        void Resize();

        void Initialize();

        Engine::Config m_config;

        usize m_FIF        = 0;
        usize m_frameIndex = 0;

        Util::FrameCounter m_frameCounter = {};

        Util::DeletionQueue                                   m_globalDeletionQueue = {};
        std::array<Util::DeletionQueue, Vk::FRAMES_IN_FLIGHT> m_deletionQueues      = {};

        Engine::Window m_window;
        Vk::Context    m_context;

        Vk::CommandBufferAllocator                m_graphicsCmdBufferAllocator;
        std::optional<Vk::CommandBufferAllocator> m_computeCmdBufferAllocator = std::nullopt;

        Vk::Swapchain m_swapchain;

        Vk::GraphicsTimeline               m_graphicsTimeline;
        std::optional<Vk::ComputeTimeline> m_computeTimeline = std::nullopt;

        Vk::FormatHelper  m_formatHelper;

        Vk::MegaSet            m_megaSet;
        Vk::FramebufferManager m_framebufferManager;
        Models::ModelManager   m_modelManager;
        Vk::PipelineManager    m_pipelineManager;

        std::optional<Vk::AccelerationStructure> m_accelerationStructure;

        Objects::GlobalSamplers m_samplers;

        PostProcess::RenderPass m_postProcess;
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

        Culling::Dispatch    m_culling;
        AO::VBGTAO::Dispatch m_vbgtao;

        IBL::Generator m_iblGenerator;

        Buffers::MeshBuffer     m_meshBuffer;
        Buffers::IndirectBuffer m_indirectBuffer;

        Buffers::SceneBuffer                m_sceneBuffer;
        std::optional<Buffers::SceneBuffer> m_sceneBufferCompute = std::nullopt;

        std::optional<Engine::Scene> m_scene = std::nullopt;

        bool m_isSwapchainOk = true;
    };
}

#endif