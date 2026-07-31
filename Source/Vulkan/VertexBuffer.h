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

#ifndef VERTEX_BUFFER2_H
#define VERTEX_BUFFER2_H

#include "GPU/Surface.h"
#include "GPU/Vertex.h"
#include "Engine/DeletionQueue.h"
#include "Vulkan/Buffer.h"
#include "Vulkan/StagingPool.h"

namespace Vk
{
    class VertexBuffer
    {
    public:
        struct Allocation
        {
            GPU::Position*    position;
            GPU::UV*          uv;
            GPU::Vertex*      normalAndTangent;
            GPU::GeometryInfo info;
        };

        VertexBuffer::Allocation Allocate
        (
            u32 elementCount,
            VkDevice device,
            VmaAllocator allocator,
            Vk::StagingPool& stagingPool,
            Engine::DeletionQueue& deletionQueue
        );

        void Free(const GPU::GeometryInfo& info);

        void Update
        (
            const Vk::CommandBuffer& cmdBuffer,
            VkDevice device,
            VmaAllocator allocator,
            Scratch::Allocator& scratchAllocator,
            Engine::DeletionQueue& deletionQueue
        );

        [[nodiscard]] bool HasPendingUploads();

        void ImGuiDisplay(Scratch::Allocator& scratchAllocator);

        void Destroy(VmaAllocator allocator);

        Vk::Buffer positionBuffer         = {};
        Vk::Buffer uvBuffer               = {};
        Vk::Buffer normalAndTangentBuffer = {};

        u32 count;
    private:
        struct GeometryUpload
        {
            GPU::GeometryInfo info = {};

            VkBuffer     positionStagingBuffer = VK_NULL_HANDLE;
            VkDeviceSize positionSourceOffset  = 0;

            VkBuffer     uvStagingBuffer = VK_NULL_HANDLE;
            VkDeviceSize uvSourceOffset  = 0;

            VkBuffer     normalAndTangentStagingBuffer = VK_NULL_HANDLE;
            VkDeviceSize normalAndTangentSourceOffset  = 0;
        };

        struct ResizeInfo
        {
            u32                            requiredCapacity = 0;
            std::vector<GPU::GeometryInfo> blocksToCopy = {};
        };

        void MergeFreeBlocks();

        std::optional<GPU::GeometryInfo> TryToFindFreeBlock(u32 elementCount);

        GPU::GeometryInfo AppendAtEnd(u32 elementCount);

        void Resize
        (
            const Vk::CommandBuffer& cmdBuffer,
            VkDevice device,
            VmaAllocator allocator,
            Scratch::Allocator& scratchAllocator,
            Engine::DeletionQueue& deletionQueue
        );

        void DisplayMemoryMapAndStatistics(Scratch::Allocator& scratchAllocator);

        std::optional<VertexBuffer::ResizeInfo> m_resizeInfo;

        std::vector<GPU::GeometryInfo> m_usedBlocks = {};
        std::vector<GPU::GeometryInfo> m_freeBlocks = {};

        std::vector<VertexBuffer::GeometryUpload> m_pendingUploads = {};

        std::mutex m_mutex;
    };
}

#endif
