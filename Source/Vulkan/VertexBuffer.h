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
#include "Externals/VMA.h"
#include "Util/Types.h"
#include "Util/DeletionQueue.h"
#include "Util/Concept.h"
#include "GPU/Vertex.h"
#include "GPU/Surface.h"

namespace Vk
{
    namespace Detail
    {
        struct GeometryUpload
        {
            GPU::GeometryInfo info   = {};
            Vk::Buffer        buffer = {};
        };

        struct VertexBufferInfo
        {
            VkBufferUsageFlags    usage      = 0;
            VkPipelineStageFlags2 stageMask  = VK_PIPELINE_STAGE_2_NONE;
            VkAccessFlags2        accessMask = VK_ACCESS_2_NONE;
        };

        template <typename T> requires GPU::IsVertexType<T>
        consteval Detail::VertexBufferInfo GetVertexBufferInfo()
        {
            Detail::VertexBufferInfo bufferInfo = {};

            if constexpr (std::is_same_v<T, GPU::Index>)
            {
                bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                   VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                   VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

                bufferInfo.stageMask = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT |
                                       VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR |
                                       VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;

                bufferInfo.accessMask = VK_ACCESS_2_INDEX_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT;
            }
            else if constexpr (std::is_same_v<T, GPU::Position>)
            {
                bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                   VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

                bufferInfo.stageMask  = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
                bufferInfo.accessMask = VK_ACCESS_2_SHADER_READ_BIT;
            }
            else if constexpr (std::is_same_v<T, GPU::UV>)
            {
                bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

                bufferInfo.stageMask  = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
                bufferInfo.accessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
            }
            else if constexpr (std::is_same_v<T, GPU::Vertex>)
            {
                bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

                bufferInfo.stageMask  = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
                bufferInfo.accessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
            }
            else
            {
                static_assert(Util::AlwaysFalse<T>, "Unsupported vertex type!");
            }

            return bufferInfo;
        }
    }

    template<typename T> requires GPU::IsVertexType<T>
    class VertexBuffer
    {
    public:
        struct WriteHandle
        {
            T*                pointer;
            GPU::GeometryInfo info;
        };

        VertexBuffer();

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
        Vk::BlockAllocator m_allocator = {};

        std::vector<Detail::GeometryUpload> m_pendingUploads = {};

        Vk::BarrierWriter m_barrierWriter = {};
    };
}

#endif
