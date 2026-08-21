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

#include "RenderConfig.h"

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
#include "TAA/RenderPass.h"
#include "SpotShadow/RenderPass.h"
#include "Debug/RenderPass.h"

#include "Culling/Dispatch.h"
#include "TiledLighting/Dispatch.h"
#include "Exposure/Dispatch.h"
#include "AO/VBAO/Dispatch.h"

#include "ShadowRT/RayDispatch.h"

#include "IBL/Generator.h"

#include "Vulkan/Context.h"
#include "Vulkan/MegaSet.h"
#include "Vulkan/FramebufferManager.h"
#include "Vulkan/AccelerationStructure.h"
#include "Vulkan/CommandBufferAllocator.h"
#include "Vulkan/GraphicsTimeline.h"
#include "Vulkan/ComputeTimeline.h"
#include "Vulkan/PipelineManager.h"
#include "Vulkan/StagingPool.h"

#include "Util/Types.h"

#include "Engine/FrameCounter.h"
#include "Engine/Window.h"
#include "Engine/SceneEditor.h"
#include "Engine/Scratch.h"

#include "Models/ModelManager.h"

#ifdef ENGINE_DLSS
#include "DLSS/Evaluation.h"
#endif

namespace Renderer
{
    class RenderManager
    {
    public:
        explicit RenderManager(Scratch::Allocator& scratchAllocator);

        ~RenderManager();

        RenderManager(const RenderManager&) noexcept = delete;
        RenderManager& operator=(const RenderManager&) noexcept = delete;

        RenderManager(RenderManager&& other) noexcept = delete;
        RenderManager& operator=(RenderManager&& other) noexcept = delete;

        void Render(Scratch::Allocator& scratchAllocator);

        [[nodiscard]] bool HandleEvents(Scratch::Allocator& scratchAllocator);
    private:
        struct AsyncComputeData
        {
            Vk::CommandBufferAllocator cmdBufferAllocator;
            Vk::ComputeTimeline        timeline;
        };

        void WaitForTimeline();
        void AcquireSwapchainImage();
        void BeginFrame();

        void RenderGraphicsQueueOnly(Scratch::Allocator& scratchAllocator);
        void RenderMultiQueue(Scratch::Allocator& scratchAllocator);

        void GBufferGeneration(const Vk::CommandBuffer& cmdBuffer, Scratch::Allocator& scratchAllocator);

        void Occlusion
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Buffers::SceneBuffer::Buffers& sceneBuffers,
            const std::string_view sceneDepthID,
            const std::string_view gNormalID,
            Scratch::Allocator& scratchAllocator
        );

        void TraceRays(const Vk::CommandBuffer& cmdBuffer);
        void Lighting(const Vk::CommandBuffer& cmdBuffer, Scratch::Allocator& scratchAllocator);
        void AntiAliasing(const Vk::CommandBuffer& cmdBuffer, Scratch::Allocator& scratchAllocator);
        void BlitToSwapchain(const Vk::CommandBuffer& cmdBuffer, Scratch::Allocator& scratchAllocator);

        void Update(const Vk::CommandBuffer& cmdBuffer, Scratch::Allocator& scratchAllocator);
        void ImGuiDisplay(Scratch::Allocator& scratchAllocator);

        void EndFrame();

        void Resize(Scratch::Allocator& scratchAllocator);

        void InitImGui(Scratch::Allocator& scratchAllocator);

        tf::Executor m_executor;

        Engine::Config m_config;

        usize m_FIF        = 0;
        usize m_frameIndex = 0;

        Engine::FrameCounter m_frameCounter = {};

        Engine::DeletionQueue                                   m_globalDeletionQueue = {};
        std::array<Engine::DeletionQueue, Vk::FRAMES_IN_FLIGHT> m_deletionQueues      = {};

        Engine::Window m_window;
        Engine::Inputs m_inputs;

        Vk::Context m_context;

        Renderer::RenderConfig m_renderConfig;

        Vk::Swapchain m_swapchain;

        Vk::CommandBufferAllocator m_graphicsCmdBufferAllocator;
        Vk::GraphicsTimeline       m_graphicsTimeline;

        std::optional<AsyncComputeData> m_asyncCompute = std::nullopt;

        Vk::MegaSet            m_megaSet;
        Vk::StagingPool        m_stagingPool;
        Vk::FramebufferManager m_framebufferManager;
        Vk::GeometryBuffer     m_geometryBuffer;
        Vk::TextureManager     m_textureManager;
        Models::ModelManager   m_modelManager;
        Vk::PipelineManager    m_pipelineManager;
        Vk::ImageDownloader    m_imageDownloader;

        std::optional<Vk::AccelerationStructure> m_accelerationStructure = std::nullopt;

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

        Engine::SceneEditor m_sceneEditor;

        bool m_isSwapchainOk = true;
    };
}

#endif