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

#ifndef DEBUG_RENDER_PASS_H
#define DEBUG_RENDER_PASS_H

#include "Renderer/Buffers/IndirectBuffer.h"
#include "Renderer/Buffers/MeshBuffer.h"
#include "Renderer/Buffers/SceneBuffer.h"
#include "Renderer/Buffers/TileLightIndexBuffer.h"
#include "Vulkan/FormatHelper.h"
#include "Vulkan/FramebufferManager.h"
#include "Vulkan/PipelineManager.h"

namespace Renderer::Debug
{
    class RenderPass
    {
    public:
        RenderPass
        (
            VkDevice device,
            VmaAllocator allocator,
            const Vk::FormatHelper& formatHelper,
            const Vk::Swapchain& swapchain,
            Vk::PipelineManager& pipelineManager,
            Vk::FramebufferManager& framebufferManager,
            Vk::StagingPool& stagingPool
        );

        void Render
        (
            usize FIF,
            usize frameIndex,
            VkDevice device,
            VmaAllocator allocator,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::Swapchain& swapchain,
            const Buffers::SceneBuffer& sceneBuffer,
            const Buffers::MeshBuffer& meshBuffer,
            const Buffers::IndirectBuffer& indirectBuffer,
            const Buffers::TileLightIndexBuffer& tiledLightIndexBuffer,
            Vk::StagingPool& stagingPool,
            Scratch::Allocator& scratchAllocator,
            Engine::DeletionQueue& deletionQueue
        );

        void Destroy(VmaAllocator allocator, Vk::StagingPool& stagingPool);
    private:
        struct AABBDebugOption
        {
            bool      enabled = true;
            glm::vec3 color   = {};
        };

        struct AABBRenderOption
        {
            AABBDebugOption singleSided = {};
            AABBDebugOption doubleSided = {};
        };

        void UploadData
        (
            const Vk::CommandBuffer& cmdBuffer,
            Vk::StagingPool& stagingPool,
            Engine::DeletionQueue& deletionQueue
        );

        void BeginDebugRender
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::FramebufferManager& framebufferManager,
            const Vk::Swapchain& swapchain
        );

        void EndDebugRender(const Vk::CommandBuffer& cmdBuffer, const Vk::FramebufferManager& framebufferManager);

        void GenerateAABBDrawCalls
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Buffers::IndirectBuffer& indirectBuffer,
            Scratch::Allocator& scratchAllocator
        );

        void RenderDebugAABB
        (
            usize FIF,
            usize frameIndex,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Buffers::SceneBuffer& sceneBuffer,
            const Buffers::MeshBuffer& meshBuffer,
            const Buffers::IndirectBuffer& indirectBuffer
        );

        void RenderDebugPointLight
        (
            usize FIF,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Buffers::SceneBuffer& sceneBuffer
        );

        void RenderDebugSpotLight
        (
            usize FIF,
            VkDevice device,
            VmaAllocator allocator,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Buffers::SceneBuffer& sceneBuffer,
            Engine::DeletionQueue& deletionQueue
        );

        void GenerateCullingStatistics
        (
            usize FIF,
            usize frameIndex,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Buffers::MeshBuffer& meshBuffer,
            const Buffers::IndirectBuffer& indirectBuffer,
            Scratch::Allocator& scratchAllocator
        );

        void GenerateTiledLightingStatistics
        (
            usize FIF,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Vk::FramebufferManager& framebufferManager,
            const Buffers::SceneBuffer& sceneBuffer,
            const Buffers::TileLightIndexBuffer& tileLightIndexBuffer
        );

        Vk::Buffer m_aabbIndexBuffer    = {};
        Vk::Buffer m_aabbDrawCallBuffer = {};

        Vk::Buffer m_sphereIndexBuffer  = {};
        Vk::Buffer m_sphereVertexBuffer = {};

        Vk::Buffer m_coneIndexBuffer  = {};
        Vk::Buffer m_coneVertexBuffer = {};

        Vk::Buffer m_cullingStatisticsBuffer       = {};
        Vk::Buffer m_tiledLightingStatisticsBuffer = {};

        std::array<Vk::Buffer, Vk::FRAMES_IN_FLIGHT> m_cullingStatisticsReadbackBuffers       = {};
        std::array<Vk::Buffer, Vk::FRAMES_IN_FLIGHT> m_tiledLightingStatisticsReadbackBuffers = {};

        std::optional<Vk::StagingMemoryBlock> m_pendingAABBIndexUpload    = std::nullopt;
        std::optional<Vk::StagingMemoryBlock> m_pendingSphereIndexUpload  = std::nullopt;
        std::optional<Vk::StagingMemoryBlock> m_pendingSphereVertexUpload = std::nullopt;

        struct AABBOptions
        {
            bool enabled = false;

            AABBRenderOption opaque = {
                .singleSided = {
                    .enabled = true,
                    .color   = {1.0f, 0.0f, 0.0f}
                },
                .doubleSided = {
                    .enabled = true,
                    .color   = {0.0f, 1.0f, 0.0f}
                }
            };

            AABBRenderOption alphaMasked = {
                .singleSided = {
                    .enabled = true,
                    .color   = {0.0f, 0.0f, 1.0f}
                },
                .doubleSided = {
                    .enabled = true,
                    .color   = {0.5f, 1.0f, 1.0f}
                }
            };
        } m_aabbDebugOptions = {};

        bool m_enablePointLightDebug         = false;
        bool m_enableSpotLightDebug          = false;
        bool m_enableCullingStatistics       = false;
        bool m_enableTiledLightingStatistics = false;
    };
}

#endif
