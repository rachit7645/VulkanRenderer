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

#ifndef GEOMETRY_BUFFER_H
#define GEOMETRY_BUFFER_H

#include <span>
#include <vulkan/vulkan.h>

#include "Buffer.h"
#include "CommandBuffer.h"
#include "VertexBuffer.h"
#include "Util/DeletionQueue.h"

namespace Vk
{
    class GeometryBuffer
    {
    public:
        explicit GeometryBuffer(const Vk::Context& context);

        void Bind(const Vk::CommandBuffer& cmdBuffer) const;
        void Destroy(VmaAllocator allocator);

        void Update
        (
            const Vk::CommandBuffer& cmdBuffer,
            VkDevice device,
            VmaAllocator allocator,
            Util::DeletionQueue& deletionQueue
        );

        void Free(const GPU::SurfaceInfo& info, Util::DeletionQueue& deletionQueue);

        void ImGuiDisplay() const;

        [[nodiscard]] bool HasPendingUploads() const;

        [[nodiscard]] const Vk::Buffer& GetIndexBuffer()    const;
        [[nodiscard]] const Vk::Buffer& GetPositionBuffer() const;
        [[nodiscard]] const Vk::Buffer& GetUVBuffer()       const;
        [[nodiscard]] const Vk::Buffer& GetVertexBuffer()   const;

        Vk::VertexBuffer<GPU::Index>    indexBuffer;
        Vk::VertexBuffer<GPU::Position> positionBuffer;
        Vk::VertexBuffer<GPU::UV>       uvBuffer;
        Vk::VertexBuffer<GPU::Vertex>   vertexBuffer;

        Vk::Buffer cubeBuffer;
    private:
        void SetupCubeUpload(VmaAllocator allocator);

        std::optional<Vk::Buffer> m_pendingCubeUpload;
    };
}

#endif
