// Copyright (c) 2023 - 2025 Rachit
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "GeometryBuffer.h"

#include <Models/Model.h>

#include "Util/Log.h"

namespace Vk
{
    // TODO: Implement resizing
    constexpr VkDeviceSize INITIAL_VERTEX_SIZE = (1 << 22) * sizeof(Models::Vertex);
    constexpr VkDeviceSize INITIAL_INDEX_SIZE  = (1 << 25) * sizeof(Models::Index);
    constexpr VkDeviceSize STAGING_BUFFER_SIZE = 32 * 1024 * 1024;

    GeometryBuffer::GeometryBuffer(VkDevice device, VmaAllocator allocator)
    {
        indexBuffer = Vk::Buffer
        (
            allocator,
            INITIAL_INDEX_SIZE,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO
        );

        vertexBuffer = Vk::Buffer
        (
            allocator,
            INITIAL_VERTEX_SIZE,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO
        );

        vertexBuffer.GetDeviceAddress(device);

        m_stagingBuffer = Vk::Buffer
        (
            allocator,
            STAGING_BUFFER_SIZE,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            VMA_MEMORY_USAGE_AUTO
        );
    }

    void GeometryBuffer::Bind(const Vk::CommandBuffer& cmdBuffer) const
    {
        vkCmdBindIndexBuffer(cmdBuffer.handle, indexBuffer.handle, 0, indexType);
    }

    Models::Info GeometryBuffer::LoadIndices(const Vk::Context& context, const std::span<const Models::Index> indices)
    {
        Models::Info indexInfo = {};

        Vk::ImmediateSubmit(context, [&](const Vk::CommandBuffer& cmdBuffer)
        {
            indexInfo = UploadIndices(cmdBuffer, indices);
        });

        return indexInfo;
    }

    Models::Info GeometryBuffer::LoadVertices(const Vk::Context& context, const std::span<const Models::Vertex> vertices)
    {
        Models::Info vertexInfo = {};

        Vk::ImmediateSubmit(context, [&](const Vk::CommandBuffer& cmdBuffer)
        {
            vertexInfo = UploadVertices(cmdBuffer, vertices);
        });

        return vertexInfo;
    }

    Models::Info GeometryBuffer::UploadIndices(const Vk::CommandBuffer& cmdBuffer, const std::span<const Models::Index> indices)
    {
        // Bytes
        const VkDeviceSize sizeBytes   = indices.size() * sizeof(Models::Index);
        const VkDeviceSize offsetBytes = indexCount     * sizeof(Models::Index);

        CheckSize(sizeBytes, offsetBytes, indexBuffer.allocInfo.size);

        std::memcpy(m_stagingBuffer.allocInfo.pMappedData, indices.data(), sizeBytes);

        const VkBufferCopy2 copyRegion =
        {
            .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
            .pNext     = nullptr,
            .srcOffset = 0,
            .dstOffset = offsetBytes,
            .size      = sizeBytes
        };

        const VkCopyBufferInfo2 copyInfo =
        {
            .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
            .pNext       = nullptr,
            .srcBuffer   = m_stagingBuffer.handle,
            .dstBuffer   = indexBuffer.handle,
            .regionCount = 1,
            .pRegions    = &copyRegion
        };

        vkCmdCopyBuffer2(cmdBuffer.handle, &copyInfo);

        indexBuffer.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
            VK_ACCESS_2_INDEX_READ_BIT,
            offsetBytes,
            sizeBytes
        );

        const Models::Info info =
        {
            .offset = indexCount,
            .count  = static_cast<u32>(indices.size())
        };

        indexCount += info.count;

        return info;
    }

    Models::Info GeometryBuffer::UploadVertices(const Vk::CommandBuffer& cmdBuffer, const std::span<const Models::Vertex> vertices)
    {
        // Bytes
        const VkDeviceSize sizeBytes   = vertices.size() * sizeof(Models::Vertex);
        const VkDeviceSize offsetBytes = vertexCount     * sizeof(Models::Vertex);

        CheckSize(sizeBytes, offsetBytes, vertexBuffer.allocInfo.size);

        std::memcpy(m_stagingBuffer.allocInfo.pMappedData, vertices.data(), sizeBytes);

        const VkBufferCopy2 copyRegion =
        {
            .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
            .pNext     = nullptr,
            .srcOffset = 0,
            .dstOffset = offsetBytes,
            .size      = sizeBytes
        };

        const VkCopyBufferInfo2 copyInfo =
        {
            .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
            .pNext       = nullptr,
            .srcBuffer   = m_stagingBuffer.handle,
            .dstBuffer   = vertexBuffer.handle,
            .regionCount = 1,
            .pRegions    = &copyRegion
        };

        vkCmdCopyBuffer2(cmdBuffer.handle, &copyInfo);

        vertexBuffer.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
            offsetBytes,
            sizeBytes
        );

        const Models::Info info =
        {
            .offset = vertexCount,
            .count  = static_cast<u32>(vertices.size())
        };

        vertexCount += info.count;

        return info;
    }

    void GeometryBuffer::CheckSize(usize sizeBytes, usize offsetBytes, usize bufferSize)
    {
        if (sizeBytes > m_stagingBuffer.allocInfo.size)
        {
            Logger::Error
            (
                "Not enough space in staging buffer! [Required={}] [Available={}]\n",
                sizeBytes,
                m_stagingBuffer.allocInfo.size
            );
        }

        if ((sizeBytes + offsetBytes) > bufferSize)
        {
            Logger::Error
            (
                "Not enough space in buffer! [Required={}] [Available={}]\n",
                sizeBytes,
                bufferSize - offsetBytes
            );
        }
    }

    void GeometryBuffer::Destroy(VmaAllocator allocator) const
    {
        indexBuffer.Destroy(allocator);
        vertexBuffer.Destroy(allocator);
        m_stagingBuffer.Destroy(allocator);
    }
}