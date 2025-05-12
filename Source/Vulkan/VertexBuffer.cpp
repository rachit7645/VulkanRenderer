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
    constexpr f64 BUFFER_GROWTH_FACTOR = 1.5;

    template<typename T>
    using WriteHandle = typename VertexBuffer<T>::WriteHandle;

    template <typename T> requires Models::IsVertexType<T>
    void VertexBuffer<T>::Bind(const Vk::CommandBuffer& cmdBuffer) const requires std::is_same_v<T, Models::Index>
    {
        vkCmdBindIndexBuffer
        (
            cmdBuffer.handle,
            buffer.handle,
            0,
            VK_INDEX_TYPE_UINT32
        );
    }

    template <typename T> requires Models::IsVertexType<T>
    void VertexBuffer<T>::Destroy(VmaAllocator allocator)
    {
        buffer.Destroy(allocator);
        m_pendingUploads.clear();
    }

    template <typename T> requires Models::IsVertexType<T>
    typename VertexBuffer<T>::WriteHandle VertexBuffer<T>::GetWriteHandle
    (
        VmaAllocator allocator,
        usize writeCount,
        Util::DeletionQueue& deletionQueue
    )
    {
        const auto stagingBuffer = Vk::Buffer
        (
            allocator,
            writeCount * sizeof(T),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            VMA_MEMORY_USAGE_AUTO
        );

        const auto info = Vk::GeometryInfo
        {
            .offset = count,
            .count  = static_cast<u32>(writeCount)
        };

        m_pendingUploads.emplace_back(info, stagingBuffer);
        count += writeCount;

        deletionQueue.PushDeletor([allocator, buffer = stagingBuffer] () mutable
        {
            buffer.Destroy(allocator);
        });

        return Vk::WriteHandle<T>
        {
            .pointer = static_cast<T*>(stagingBuffer.allocationInfo.pMappedData),
            .info    = info
        };
    }

    template <typename T> requires Models::IsVertexType<T>
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

        ResizeBuffer
        (
            cmdBuffer,
            device,
            allocator,
            deletionQueue
        );

        // Copy
        {
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
                    .dstBuffer   = buffer.handle,
                    .regionCount = 1,
                    .pRegions    = &copyRegion
                };

                vkCmdCopyBuffer2(cmdBuffer.handle, &copyInfo);
            }
        }

        // Barriers
        {
            VkPipelineStageFlags2 dstStageMask  = VK_PIPELINE_STAGE_2_NONE;
            VkAccessFlags2        dstAccessMask = VK_ACCESS_2_NONE;

            if constexpr (std::is_same_v<T, Models::Index>)
            {
                dstStageMask  = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT | VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
                dstAccessMask = VK_ACCESS_2_INDEX_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT;
            }
            else if constexpr (std::is_same_v<T, Models::Position>)
            {
                dstStageMask  = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
                dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
            }
            else if constexpr (std::is_same_v<T, Models::Vertex>)
            {
                dstStageMask  = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
                dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
            }

            for (const auto& [info, _] : m_pendingUploads)
            {
                m_barrierWriter.WriteBufferBarrier
                (
                    buffer,
                    Vk::BufferBarrier{
                        .srcStageMask  = VK_PIPELINE_STAGE_2_COPY_BIT,
                        .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                        .dstStageMask  = dstStageMask,
                        .dstAccessMask = dstAccessMask,
                        .offset        = info.offset * sizeof(T),
                        .size          = info.count  * sizeof(T)
                    }
                );
            }

            m_barrierWriter.Execute(cmdBuffer);
        }

        m_pendingUploads.clear();
    }

    template <typename T> requires Models::IsVertexType<T>
    bool VertexBuffer<T>::HasPendingUploads() const
    {
        return !m_pendingUploads.empty();
    }

    template <typename T> requires Models::IsVertexType<T>
    void VertexBuffer<T>::ResizeBuffer
    (
        const Vk::CommandBuffer& cmdBuffer,
        VkDevice device,
        VmaAllocator allocator,
        Util::DeletionQueue& deletionQueue
    )
    {
        if (count == 0)
        {
            return;
        }

        VkBufferUsageFlags    usage         = 0;
        VkPipelineStageFlags2 srcStageMask  = VK_PIPELINE_STAGE_2_NONE;
        VkAccessFlags2        srcAccessMask = VK_ACCESS_2_NONE;

        if constexpr (std::is_same_v<T, Models::Index>)
        {
            usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                    VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

            srcStageMask  = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
            srcAccessMask = VK_ACCESS_2_INDEX_READ_BIT;
        }
        else if constexpr (std::is_same_v<T, Models::Position>)
        {
            usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

            srcStageMask  = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
            srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
        }
        else if constexpr (std::is_same_v<T, Models::Vertex>)
        {
            usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

            srcStageMask  = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
            srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
        }

        if (count * sizeof(T) > buffer.requestedSize)
        {
            u32 pendingCount = 0;

            for (const auto& upload : m_pendingUploads)
            {
                pendingCount += upload.info.count;
            }

            if (pendingCount > count)
            {
                Logger::Error("{}\n", "Pending m_count exceeds total m_count!");
            }

            const u32          oldCount = count - pendingCount;
            const VkDeviceSize oldSize  = oldCount * sizeof(T);

            auto oldBuffer = buffer;

            const auto newSize = static_cast<VkDeviceSize>(static_cast<f64>(count * sizeof(T)) * BUFFER_GROWTH_FACTOR);

            buffer = Vk::Buffer
            (
                allocator,
                newSize,
                usage,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                0,
                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
            );

            buffer.GetDeviceAddress(device);

            if (oldBuffer.handle != VK_NULL_HANDLE && oldCount > 0)
            {
                oldBuffer.Barrier
                (
                    cmdBuffer,
                    Vk::BufferBarrier{
                        .srcStageMask  = srcStageMask,
                        .srcAccessMask = srcAccessMask,
                        .dstStageMask  = VK_PIPELINE_STAGE_2_COPY_BIT,
                        .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
                        .offset        = 0,
                        .size          = oldSize
                    }
                );

                const VkBufferCopy2 copyRegion =
                {
                    .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                    .pNext     = nullptr,
                    .srcOffset = 0,
                    .dstOffset = 0,
                    .size      = oldSize
                };

                const VkCopyBufferInfo2 copyInfo =
                {
                    .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
                    .pNext       = nullptr,
                    .srcBuffer   = oldBuffer.handle,
                    .dstBuffer   = buffer.handle,
                    .regionCount = 1,
                    .pRegions    = &copyRegion,
                };

                vkCmdCopyBuffer2(cmdBuffer.handle, &copyInfo);
            }

            deletionQueue.PushDeletor([allocator, buffer = oldBuffer] () mutable
            {
                buffer.Destroy(allocator);
            });
        }
    }

    // Explicit Instantiations
    template class Vk::VertexBuffer<Models::Index>;
    template class Vk::VertexBuffer<Models::Position>;
    template class Vk::VertexBuffer<Models::Vertex>;
}