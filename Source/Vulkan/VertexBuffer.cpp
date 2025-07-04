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

#include "VertexBuffer.h"

#include "Util/Log.h"

namespace Vk
{
    template<typename T>
    using WriteHandle = typename VertexBuffer<T>::WriteHandle;

    template <typename T> requires GPU::IsVertexType<T>
    VertexBuffer<T>::VertexBuffer(const Vk::Extensions& extensions)
    {
        if constexpr (std::is_same_v<T, GPU::Index>)
        {
            m_usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

            m_stageMask = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;

            m_accessMask = VK_ACCESS_2_INDEX_READ_BIT;

            if (extensions.HasRayTracing())
            {
                m_usage      |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
                m_stageMask  |= VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
                m_accessMask |= VK_ACCESS_2_SHADER_READ_BIT;
            }
        }
        else if constexpr (std::is_same_v<T, GPU::Position>)
        {
            m_usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

            m_stageMask  = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
            m_accessMask = VK_ACCESS_2_SHADER_READ_BIT;

            if (extensions.HasRayTracing())
            {
                m_usage     |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
                m_stageMask |= VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
            }
        }
        else if constexpr (std::is_same_v<T, GPU::UV>)
        {
            m_usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

            m_stageMask  = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
            m_accessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;

            if (extensions.HasRayTracing())
            {
                m_stageMask |= VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
            }
        }
        else if constexpr (std::is_same_v<T, GPU::Vertex>)
        {
            m_usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                      VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

            m_stageMask  = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
            m_accessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
        }
        else
        {
            static_assert(Util::AlwaysFalse<T>, "Unsupported vertex type!");
        }

        m_allocator = Vk::BlockAllocator(m_usage, m_stageMask, m_accessMask);
    }

    template <typename T> requires GPU::IsVertexType<T>
    void VertexBuffer<T>::Bind(const Vk::CommandBuffer& cmdBuffer) const requires std::is_same_v<T, GPU::Index>
    {
        vkCmdBindIndexBuffer
        (
            cmdBuffer.handle,
            m_allocator.buffer.handle,
            0,
            VK_INDEX_TYPE_UINT32
        );
    }

    template <typename T> requires GPU::IsVertexType<T>
    void VertexBuffer<T>::Destroy(VmaAllocator allocator)
    {
        m_allocator.Destroy(allocator);
    }

    template <typename T> requires GPU::IsVertexType<T>
    typename VertexBuffer<T>::WriteHandle VertexBuffer<T>::Allocate
    (
        VmaAllocator allocator,
        usize writeCount,
        Util::DeletionQueue& deletionQueue
    )
    {
        const VkDeviceSize writeSize = writeCount * sizeof(T);

        const auto stagingBuffer = Vk::Buffer
        (
            allocator,
            writeSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            VMA_MEMORY_USAGE_AUTO
        );

        deletionQueue.PushDeletor([allocator, buffer = stagingBuffer] () mutable
        {
            buffer.Destroy(allocator);
        });

        const auto allocation = m_allocator.Allocate(writeSize);

        const auto info = GPU::GeometryInfo
        {
            .offset = static_cast<u32>(allocation.offset / sizeof(T)),
            .count  = static_cast<u32>(allocation.size   / sizeof(T))
        };

        count += info.count;

        m_pendingUploads.emplace_back(info, stagingBuffer);

        return Vk::WriteHandle<T>
        {
            .pointer = static_cast<T*>(stagingBuffer.allocationInfo.pMappedData),
            .info    = info
        };
    }

    template <typename T> requires GPU::IsVertexType<T>
    void VertexBuffer<T>::Free(const GPU::GeometryInfo& info)
    {
        const auto block = BlockAllocator::Block
        {
            .offset = info.offset * sizeof(T),
            .size   = info.count  * sizeof(T)
        };

        m_allocator.Free(block);

        if (count < info.count)
        {
            Logger::Warning("Suspicious free! [Offset={}] [Count={}]", info.offset, info.count);

            count = 0;
        }
        else
        {
            count -= info.count;
        }
    }

    template <typename T> requires GPU::IsVertexType<T>
    void VertexBuffer<T>::FlushUploads
    (
        const Vk::CommandBuffer& cmdBuffer,
        VkDevice device,
        VmaAllocator allocator,
        Util::DeletionQueue& deletionQueue
    )
    {
        if (!HasPendingUploads())
        {
            return;
        }

        m_allocator.Update
        (
            cmdBuffer,
            device,
            allocator,
            deletionQueue
        );

        for (const auto& [info, _] : m_pendingUploads)
        {
            m_barrierWriter.WriteBufferBarrier
            (
               m_allocator.buffer,
               Vk::BufferBarrier{
                   .srcStageMask   = m_stageMask,
                   .srcAccessMask  = m_accessMask,
                   .dstStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                   .dstAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                   .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                   .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                   .offset         = info.offset * sizeof(T),
                   .size           = info.count  * sizeof(T)
               }
            );
        }

        m_barrierWriter.Execute(cmdBuffer);

        for (const auto& [info, stagingBuffer] : m_pendingUploads)
        {
            const VkBufferCopy2 copyRegion =
            {
                .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                .pNext     = nullptr,
                .srcOffset = 0,
                .dstOffset = info.offset * sizeof(T),
                .size      = info.count  * sizeof(T)
            };

            const VkCopyBufferInfo2 copyInfo =
            {
                .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
                .pNext       = nullptr,
                .srcBuffer   = stagingBuffer.handle,
                .dstBuffer   = m_allocator.buffer.handle,
                .regionCount = 1,
                .pRegions    = &copyRegion
            };

            vkCmdCopyBuffer2(cmdBuffer.handle, &copyInfo);

            m_barrierWriter.WriteBufferBarrier
            (
                m_allocator.buffer,
                Vk::BufferBarrier{
                    .srcStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                    .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .dstStageMask   = m_stageMask,
                    .dstAccessMask  = m_accessMask,
                    .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .offset         = info.offset * sizeof(T),
                    .size           = info.count  * sizeof(T)
                }
            );
        }

        m_barrierWriter.Execute(cmdBuffer);

        m_pendingUploads.clear();
    }

    template <typename T> requires GPU::IsVertexType<T>
    bool VertexBuffer<T>::HasPendingUploads() const
    {
        return !m_pendingUploads.empty();
    }

    template <typename T> requires GPU::IsVertexType<T>
    const Vk::Buffer& VertexBuffer<T>::GetBuffer() const
    {
        return m_allocator.buffer;
    }

    // Explicit Instantiations
    template class Vk::VertexBuffer<GPU::Index>;
    template class Vk::VertexBuffer<GPU::Position>;
    template class Vk::VertexBuffer<GPU::UV>;
    template class Vk::VertexBuffer<GPU::Vertex>;
}