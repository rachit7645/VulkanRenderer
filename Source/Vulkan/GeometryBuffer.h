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

#ifndef GEOMETRY_BUFFER_H
#define GEOMETRY_BUFFER_H

#include <vulkan/vulkan.h>

#include "Buffer.h"
#include "CommandBuffer.h"
#include "IndexBuffer.h"
#include "Context.h"
#include "Engine/DeletionQueue.h"
#include "Vulkan/VertexBuffer.h"

namespace Vk
{
    class GeometryBuffer
    {
    public:
        GeometryBuffer(const Vk::Context& context, Vk::StagingPool& stagingPool);

        void Bind(const Vk::CommandBuffer& cmdBuffer) const;
        void Destroy(VmaAllocator allocator, Vk::StagingPool& stagingPool);

        void Update
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::Context& context,
            Vk::StagingPool& stagingPool,
            Scratch::Allocator& scratchAllocator,
            Engine::DeletionQueue& deletionQueue
        );

        void Free(const GPU::SurfaceInfo& info, Engine::DeletionQueue& deletionQueue);

        void ImGuiDisplay(Scratch::Allocator& scratchAllocator);

        [[nodiscard]] bool HasPendingUploads();

        [[nodiscard]] const Vk::Buffer& GetIndexBuffer()    const;
        [[nodiscard]] const Vk::Buffer& GetPositionBuffer() const;
        [[nodiscard]] const Vk::Buffer& GetUVBuffer()       const;
        [[nodiscard]] const Vk::Buffer& GetVertexBuffer()   const;

        Vk::IndexBuffer  indexBuffer  = {};
        Vk::VertexBuffer vertexBuffer = {};

        Vk::Buffer cubeBuffer;
    private:
        void SetupCubeUpload(VkDevice device, VmaAllocator allocator, Vk::StagingPool& stagingPool);

        std::optional<Vk::StagingMemoryBlock> m_pendingCubeUpload;
    };
}

#endif
