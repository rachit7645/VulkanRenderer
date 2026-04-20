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
            const Vk::Swapchain& swapchain,
            Vk::PipelineManager& pipelineManager,
            Vk::StagingPool& stagingPool
        );

        void Render
        (
            usize FIF,
            usize frameIndex,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Vk::Swapchain& swapchain,
            const Buffers::SceneBuffer& sceneBuffer,
            const Buffers::MeshBuffer& meshBuffer,
            const Buffers::IndirectBuffer& indirectBuffer,
            Vk::StagingPool& stagingPool,
            Util::DeletionQueue& deletionQueue
        );

        void Destroy(VmaAllocator allocator, Vk::StagingPool& stagingPool);
    private:
        void GenerateAABBDrawCalls
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Buffers::IndirectBuffer& indirectBuffer
        );

        void RenderDebugAABB
        (
            usize FIF,
            usize frameIndex,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Vk::Swapchain& swapchain,
            const Buffers::SceneBuffer& sceneBuffer,
            const Buffers::MeshBuffer& meshBuffer,
            const Buffers::IndirectBuffer& indirectBuffer
        );

        Vk::Buffer m_indexBuffer;
        Vk::Buffer m_drawCallBuffer;

        std::optional<Vk::StagingMemoryBlock> m_pendingIndexUpload = std::nullopt;

        bool m_renderAABBDebug = false;
    };
}

#endif
