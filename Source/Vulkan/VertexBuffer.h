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

#ifndef VERTEX_BUFFER_H
#define VERTEX_BUFFER_H

#include <vulkan/vulkan.h>

#include "Buffer.h"
#include "BarrierWriter.h"
#include "BlockAllocator.h"
#include "Extensions.h"
#include "Externals/VMA.h"
#include "Util/Types.h"
#include "Util/DeletionQueue.h"
#include "Util/Concept.h"
#include "GPU/Vertex.h"
#include "GPU/Surface.h"
#include "Vulkan/Context.h"

namespace Vk
{
    template<typename T> requires GPU::IsVertexType<T>
    class VertexBuffer
    {
    public:
        struct WriteHandle
        {
            T*                pointer;
            GPU::GeometryInfo info;
        };

        VertexBuffer(const Vk::Extensions& extensions);

        void Bind(const Vk::CommandBuffer& cmdBuffer) const requires std::is_same_v<T, GPU::Index>;

        void Destroy(VmaAllocator allocator);

        WriteHandle Allocate
        (
            VmaAllocator allocator,
            usize writeCount,
            Util::DeletionQueue& deletionQueue
        );

        void Free(const GPU::GeometryInfo& info);

        void FlushUploads
        (
            const Vk::CommandBuffer& cmdBuffer,
            VkDevice device,
            VmaAllocator allocator,
            Util::DeletionQueue& deletionQueue
        );

        [[nodiscard]] bool HasPendingUploads() const;

        [[nodiscard]] const Vk::Buffer& GetBuffer() const;

        u32 count = 0;
    private:
        struct GeometryUpload
        {
            GPU::GeometryInfo info   = {};
            Vk::Buffer        buffer = {};
        };

        VkBufferUsageFlags    m_usage      = 0;
        VkPipelineStageFlags2 m_stageMask  = VK_PIPELINE_STAGE_2_NONE;
        VkAccessFlags2        m_accessMask = VK_ACCESS_2_NONE;

        Vk::BlockAllocator m_allocator = {};

        std::vector<GeometryUpload> m_pendingUploads = {};

        Vk::BarrierWriter m_barrierWriter = {};
    };
}

#endif
