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
#include "Externals/VMA.h"
#include "Util/Types.h"
#include "Util/DeletionQueue.h"
#include "GPU/Vertex.h"

namespace Vk
{
    struct GeometryInfo
    {
        u32 offset = 0;
        u32 count  = 0;
    };

    struct GeometryUpload
    {
        Vk::GeometryInfo info;
        Vk::Buffer       buffer;
    };

    template<typename T> requires GPU::IsVertexType<T>
    class VertexBuffer
    {
    public:
        struct WriteHandle
        {
            T*               pointer;
            Vk::GeometryInfo info;
        };

        void Bind(const Vk::CommandBuffer& cmdBuffer) const requires std::is_same_v<T, GPU::Index>;
        void Destroy(VmaAllocator allocator);

        WriteHandle GetWriteHandle
        (
            VmaAllocator allocator,
            usize writeCount,
            Util::DeletionQueue& deletionQueue
        );

        void FlushUploads
        (
            const Vk::CommandBuffer& cmdBuffer,
            VkDevice device,
            VmaAllocator allocator,
            Util::DeletionQueue& deletionQueue
        );

        [[nodiscard]] bool HasPendingUploads() const;

        Vk::Buffer buffer = {};

        u32 count = 0;
    private:
        void ResizeBuffer
        (
            const Vk::CommandBuffer& cmdBuffer,
            VkDevice device,
            VmaAllocator allocator,
            Util::DeletionQueue& deletionQueue
        );

        std::vector<Vk::GeometryUpload> m_pendingUploads = {};

        Vk::BarrierWriter m_barrierWriter = {};
    };
}

#endif
