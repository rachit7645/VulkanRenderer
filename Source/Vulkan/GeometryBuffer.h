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
#include "Models/Vertex.h"
#include "Util/Util.h"
#include "CommandBuffer.h"

namespace Vk
{
    class GeometryBuffer
    {
    public:
        struct Info
        {
            u32 offset = 0;
            u32 count  = 0;
        };

        explicit GeometryBuffer(VkDevice device, VmaAllocator allocator);

        void Bind(const Vk::CommandBuffer& cmdBuffer) const;
        void Destroy(VmaAllocator allocator);

        template<typename T>
        std::pair<T*, GeometryBuffer::Info> GetWritePointer
        (
            VmaAllocator allocator,
            usize count,
            Util::DeletionQueue& deletionQueue
        );

        void Update
        (
            const Vk::CommandBuffer& cmdBuffer,
            VkDevice device,
            VmaAllocator allocator,
            Util::DeletionQueue& deletionQueue
        );

        void ImGuiDisplay() const;

        [[nodiscard]] bool HasPendingUploads() const;

        Vk::Buffer indexBuffer;
        Vk::Buffer positionBuffer;
        Vk::Buffer vertexBuffer;

        Vk::Buffer cubeBuffer;

        u32 indexCount    = 0;
        u32 positionCount = 0;
        u32 vertexCount   = 0;
    private:
        struct Upload
        {
            GeometryBuffer::Info info;
            Vk::Buffer           buffer;
        };

        void SetupCubeUpload(VmaAllocator allocator);

        template<typename T>
        std::pair<T*, GeometryBuffer::Info> SetupUpload
        (
            VmaAllocator allocator,
            usize count,
            u32& offset,
            std::vector<GeometryBuffer::Upload>& uploads,
            Util::DeletionQueue& deletionQueue
        );

        void ResizeBuffer
        (
            const Vk::CommandBuffer& cmdBuffer,
            VkDevice device,
            VmaAllocator allocator,
            u32 count,
            usize elementSize,
            const std::span<const Upload> uploads,
            VkBufferUsageFlags usage,
            VkPipelineStageFlags2 srcStageMask,
            VkAccessFlags2 srcAccessMask,
            Vk::Buffer& buffer,
            Util::DeletionQueue& deletionQueue
        );

        void ResizeBuffers
        (
            const Vk::CommandBuffer& cmdBuffer,
            VkDevice device,
            VmaAllocator allocator,
            Util::DeletionQueue& deletionQueue
        );

        void FlushUploads
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::Buffer& destination,
            const std::span<const Upload> uploads,
            usize elementSize,
            VkPipelineStageFlags2 dstStageMask,
            VkAccessFlags2 dstAccessMask
        );

        std::vector<Upload> m_pendingIndexUploads;
        std::vector<Upload> m_pendingPositionUploads;
        std::vector<Upload> m_pendingVertexUploads;

        std::optional<Upload> m_pendingCubeUpload;
    };
}

#endif
