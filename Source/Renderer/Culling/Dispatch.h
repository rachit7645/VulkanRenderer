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

#ifndef CULLING_DISPATCH_H
#define CULLING_DISPATCH_H

#include "FrustumBuffer.h"
#include "Renderer/Buffers/IndirectBuffer.h"
#include "Renderer/Buffers/MeshBuffer.h"
#include "Vulkan/PipelineManager.h"

namespace Renderer::Culling
{
    class Dispatch
    {
    public:
        Dispatch
        (
            VkDevice device,
            VmaAllocator allocator,
            Vk::PipelineManager& pipelineManager
        );

        void Destroy(VmaAllocator allocator);

        void Frustum
        (
            usize FIF,
            usize frameIndex,
            const glm::mat4& projectionView,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::PipelineManager& pipelineManager,
            const Buffers::MeshBuffer& meshBuffer,
            const Buffers::IndirectBuffer& indirectBuffer
        );
    private:
        bool NeedsDispatch
        (
            usize FIF,
            const Vk::CommandBuffer& cmdBuffer,
            const Buffers::IndirectBuffer& indirectBuffer
        );

        void PreDispatch
        (
            usize FIF,
            const glm::mat4& projectionView,
            const Vk::CommandBuffer& cmdBuffer,
            const Buffers::IndirectBuffer& indirectBuffer
        );

        void Execute
        (
            usize FIF,
            const Vk::CommandBuffer& cmdBuffer,
            const Buffers::IndirectBuffer& indirectBuffer
        );

        void PostDispatch
        (
            usize FIF,
            const Vk::CommandBuffer& cmdBuffer,
            const Buffers::IndirectBuffer& indirectBuffer
        );

        static u32 GetWorkGroupCount(usize FIF, const Buffers::IndirectBuffer& indirectBuffer);

        Culling::FrustumBuffer m_frustumBuffer;

        Vk::BarrierWriter m_barrierWriter = {};
    };
}

#endif
