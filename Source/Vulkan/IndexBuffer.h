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

#ifndef VERTEX_BUFFER_H
#define VERTEX_BUFFER_H

#include <vulkan/vulkan.h>

#include "Buffer.h"
#include "BarrierWriter.h"
#include "StagingPool.h"
#include "Externals/VMA.h"
#include "Util/Types.h"
#include "Engine/DeletionQueue.h"
#include "GPU/Vertex.h"
#include "GPU/Surface.h"

namespace Vk
{
    class IndexBuffer
    {
    public:
        struct Allocation
        {
            GPU::Index*       index;
            GPU::GeometryInfo info;
        };

        void Bind(const Vk::CommandBuffer& cmdBuffer) const;

        void Destroy(VmaAllocator allocator);

        IndexBuffer::Allocation Allocate
        (
            VkDevice device,
            VmaAllocator allocator,
            usize writeCount,
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

        Vk::Buffer buffer = {};

        u32 count = 0;
    private:
        struct GeometryUpload
        {
            GPU::GeometryInfo info         = {};
            VkDeviceSize      sourceOffset = 0;
        };

        struct ResizeInfo
        {
            u32                            requiredCapacity = 0;
            std::vector<GPU::GeometryInfo> blocksToCopy     = {};
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

        std::optional<IndexBuffer::ResizeInfo> m_resizeInfo;

        std::vector<GPU::GeometryInfo> m_usedBlocks = {};
        std::vector<GPU::GeometryInfo> m_freeBlocks = {};

        ankerl::unordered_dense::map<VkBuffer, std::vector<GeometryUpload>> m_pendingUploads;

        std::mutex m_mutex;
    };
}

#endif
