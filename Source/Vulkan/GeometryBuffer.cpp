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

#include "GeometryBuffer.h"

#include <volk/volk.h>

#include "Util/Log.h"
#include "Models/Model.h"
#include "DebugUtils.h"
#include "Renderer/RenderConstants.h"

namespace Vk
{
    constexpr f64 BUFFER_GROWTH_FACTOR = 1.5;

    GeometryBuffer::GeometryBuffer(VkDevice device, VmaAllocator allocator)
    {
        cubeBuffer = Vk::Buffer
        (
            allocator,
            36 * sizeof(Models::Position),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        );

        cubeBuffer.GetDeviceAddress(device);

        Vk::SetDebugName(device, cubeBuffer.handle, "GeometryBuffer/CubeBuffer");

        SetupCubeUpload(allocator);
    }

    void GeometryBuffer::Bind(const Vk::CommandBuffer& cmdBuffer) const
    {
        vkCmdBindIndexBuffer(cmdBuffer.handle, indexBuffer.handle, 0, VK_INDEX_TYPE_UINT32);
    }

    std::array<GeometryBuffer::Info, 3> GeometryBuffer::SetupUploads
    (
        VmaAllocator allocator,
        const std::span<const Models::Index> indices,
        const std::span<const Models::Position> positions,
        const std::span<const Models::Vertex> vertices
    )
    {
        const auto indexInfo = SetupUpload
        (
            allocator,
            indices,
            indexCount,
            m_pendingIndexUploads
        );

        const auto positionInfo = SetupUpload
        (
            allocator,
            positions,
            positionCount,
            m_pendingPositionUploads
        );

        const auto vertexInfo = SetupUpload
        (
            allocator,
            vertices,
            vertexCount,
            m_pendingVertexUploads
        );

        return {indexInfo, positionInfo, vertexInfo};
    }

    void GeometryBuffer::Update
    (
        const Vk::CommandBuffer& cmdBuffer,
        VkDevice device,
        VmaAllocator allocator
    )
    {
        if (!HasPendingUploads())
        {
            return;
        }

        Vk::BeginLabel(cmdBuffer, "Geometry Transfer", {0.9882f, 0.7294f, 0.0118f, 1.0f});

        ResizeBuffers(cmdBuffer, device, allocator);

        Vk::BeginLabel(cmdBuffer, "Index Transfer", {0.8901f, 0.0549f, 0.3607f, 1.0f});

        FlushUploads
        (
            cmdBuffer,
            indexBuffer,
            m_pendingIndexUploads,
            sizeof(Models::Index),
            VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
            VK_ACCESS_2_INDEX_READ_BIT
        );

        Vk::EndLabel(cmdBuffer);

        Vk::BeginLabel(cmdBuffer, "Position Transfer", {0.4039f, 0.0509f, 0.5215f, 1.0f});

        FlushUploads
        (
            cmdBuffer,
            positionBuffer,
            m_pendingPositionUploads,
            sizeof(Models::Position),
            VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT
        );

        Vk::EndLabel(cmdBuffer);

        Vk::BeginLabel(cmdBuffer, "Vertex Transfer", {0.6117f, 0.0549f, 0.8901f, 1.0f});

        FlushUploads
        (
            cmdBuffer,
            vertexBuffer,
            m_pendingVertexUploads,
            sizeof(Models::Vertex),
            VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT
        );

        Vk::EndLabel(cmdBuffer);

        if (m_cubeStagingBuffer.has_value())
        {
            Vk::BeginLabel(cmdBuffer, "Cube Transfer", {0.5117f, 0.0749f, 0.3901f, 1.0f});

            UploadToBuffer
            (
                cmdBuffer,
                m_cubeStagingBuffer.value(),
                cubeBuffer,
                0,
                36 * sizeof(glm::vec3),
                VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                VK_ACCESS_2_SHADER_STORAGE_READ_BIT
            );

            Vk::EndLabel(cmdBuffer);
        }

        Vk::EndLabel(cmdBuffer);
    }

    void GeometryBuffer::ClearUploads(VmaAllocator allocator)
    {
        m_deletionQueue.FlushQueue();

        for (auto& buffer : m_pendingIndexUploads | std::views::values)
        {
            buffer.Destroy(allocator);
        }

        for (auto& buffer : m_pendingPositionUploads | std::views::values)
        {
            buffer.Destroy(allocator);
        }

        for (auto& buffer : m_pendingVertexUploads | std::views::values)
        {
            buffer.Destroy(allocator);
        }

        if (m_cubeStagingBuffer.has_value())
        {
            m_cubeStagingBuffer->Destroy(allocator);
            m_cubeStagingBuffer = std::nullopt;
        }

        m_pendingIndexUploads.clear();
        m_pendingPositionUploads.clear();
        m_pendingVertexUploads.clear();
    }

    void GeometryBuffer::SetupCubeUpload(VmaAllocator allocator)
    {
        constexpr f32 SKYBOX_SIZE = 1.0f;

        constexpr std::array SKYBOX_VERTICES =
        {
            -SKYBOX_SIZE,  SKYBOX_SIZE, -SKYBOX_SIZE,
            -SKYBOX_SIZE, -SKYBOX_SIZE, -SKYBOX_SIZE,
             SKYBOX_SIZE, -SKYBOX_SIZE, -SKYBOX_SIZE,
             SKYBOX_SIZE, -SKYBOX_SIZE, -SKYBOX_SIZE,
             SKYBOX_SIZE,  SKYBOX_SIZE, -SKYBOX_SIZE,
            -SKYBOX_SIZE,  SKYBOX_SIZE, -SKYBOX_SIZE,

            -SKYBOX_SIZE, -SKYBOX_SIZE,  SKYBOX_SIZE,
            -SKYBOX_SIZE, -SKYBOX_SIZE, -SKYBOX_SIZE,
            -SKYBOX_SIZE,  SKYBOX_SIZE, -SKYBOX_SIZE,
            -SKYBOX_SIZE,  SKYBOX_SIZE, -SKYBOX_SIZE,
            -SKYBOX_SIZE,  SKYBOX_SIZE,  SKYBOX_SIZE,
            -SKYBOX_SIZE, -SKYBOX_SIZE,  SKYBOX_SIZE,

             SKYBOX_SIZE, -SKYBOX_SIZE, -SKYBOX_SIZE,
             SKYBOX_SIZE, -SKYBOX_SIZE,  SKYBOX_SIZE,
             SKYBOX_SIZE,  SKYBOX_SIZE,  SKYBOX_SIZE,
             SKYBOX_SIZE,  SKYBOX_SIZE,  SKYBOX_SIZE,
             SKYBOX_SIZE,  SKYBOX_SIZE, -SKYBOX_SIZE,
             SKYBOX_SIZE, -SKYBOX_SIZE, -SKYBOX_SIZE,

            -SKYBOX_SIZE, -SKYBOX_SIZE, SKYBOX_SIZE,
            -SKYBOX_SIZE,  SKYBOX_SIZE, SKYBOX_SIZE,
             SKYBOX_SIZE,  SKYBOX_SIZE, SKYBOX_SIZE,
             SKYBOX_SIZE,  SKYBOX_SIZE, SKYBOX_SIZE,
             SKYBOX_SIZE, -SKYBOX_SIZE, SKYBOX_SIZE,
            -SKYBOX_SIZE, -SKYBOX_SIZE, SKYBOX_SIZE,

            -SKYBOX_SIZE, SKYBOX_SIZE, -SKYBOX_SIZE,
             SKYBOX_SIZE, SKYBOX_SIZE, -SKYBOX_SIZE,
             SKYBOX_SIZE, SKYBOX_SIZE,  SKYBOX_SIZE,
             SKYBOX_SIZE, SKYBOX_SIZE,  SKYBOX_SIZE,
            -SKYBOX_SIZE, SKYBOX_SIZE,  SKYBOX_SIZE,
            -SKYBOX_SIZE, SKYBOX_SIZE, -SKYBOX_SIZE,

            -SKYBOX_SIZE, -SKYBOX_SIZE, -SKYBOX_SIZE,
            -SKYBOX_SIZE, -SKYBOX_SIZE,  SKYBOX_SIZE,
             SKYBOX_SIZE, -SKYBOX_SIZE, -SKYBOX_SIZE,
             SKYBOX_SIZE, -SKYBOX_SIZE, -SKYBOX_SIZE,
            -SKYBOX_SIZE, -SKYBOX_SIZE,  SKYBOX_SIZE,
             SKYBOX_SIZE, -SKYBOX_SIZE,  SKYBOX_SIZE
        };

        constexpr usize VERTICES_SIZE = SKYBOX_VERTICES.size() * sizeof(f32);

        m_cubeStagingBuffer = Vk::Buffer
        (
            allocator,
            VERTICES_SIZE,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            VMA_MEMORY_USAGE_AUTO
        );

        std::memcpy(m_cubeStagingBuffer->allocationInfo.pMappedData, SKYBOX_VERTICES.data(), VERTICES_SIZE);
    }

    template<typename T>
    GeometryBuffer::Info GeometryBuffer::SetupUpload
    (
        VmaAllocator allocator,
        const std::span<const T> data,
        u32& offset,
        std::vector<Upload>& uploads
    )
    {
        const auto stagingBuffer = Vk::Buffer
        (
            allocator,
            data.size_bytes(),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            VMA_MEMORY_USAGE_AUTO
        );

        std::memcpy(stagingBuffer.allocationInfo.pMappedData, data.data(), data.size_bytes());

        const GeometryBuffer::Info info =
        {
            .offset = offset,
            .count  = static_cast<u32>(data.size())
        };

        uploads.emplace_back(info, stagingBuffer);

        offset += data.size();

        return info;
    }

    void GeometryBuffer::ResizeBuffer
    (
        const Vk::CommandBuffer& cmdBuffer,
        VmaAllocator allocator,
        u32 count,
        const std::span<const Upload> uploads,
        usize elementSize,
        VkBufferUsageFlags usage,
        Vk::Buffer& buffer
    )
    {
        if (count == 0)
        {
            return;
        }

        if (count * elementSize > buffer.requestedSize)
        {
            u32 pendingCount = 0;

            for (const auto [_, count] : uploads | std::views::keys)
            {
                pendingCount += count;
            }

            auto oldBuffer = buffer;

            const auto newSize = static_cast<usize>(static_cast<f64>(count) * BUFFER_GROWTH_FACTOR) * elementSize;

            buffer = Vk::Buffer
            (
                allocator,
                newSize,
                usage,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                0,
                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
            );

            if (oldBuffer.handle != VK_NULL_HANDLE && (count - pendingCount) > 0)
            {
                const VkBufferCopy2 copyRegion =
                {
                    .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                    .pNext     = nullptr,
                    .srcOffset = 0,
                    .dstOffset = 0,
                    .size      = (count - pendingCount) * elementSize
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

            m_deletionQueue.PushDeletor([oldBuffer, allocator] () mutable
            {
                oldBuffer.Destroy(allocator);
            });
        }
    }

    void GeometryBuffer::ResizeBuffers
    (
        const Vk::CommandBuffer& cmdBuffer,
        VkDevice device,
        VmaAllocator allocator
    )
    {
        ResizeBuffer
        (
            cmdBuffer,
            allocator,
            indexCount,
            m_pendingIndexUploads,
            sizeof(Models::Index),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            indexBuffer
        );

        ResizeBuffer
        (
            cmdBuffer,
            allocator,
            positionCount,
            m_pendingPositionUploads,
            sizeof(Models::Position),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            positionBuffer
        );

        ResizeBuffer
        (
            cmdBuffer,
            allocator,
            vertexCount,
            m_pendingVertexUploads,
            sizeof(Models::Vertex),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            vertexBuffer
        );

        indexBuffer.GetDeviceAddress(device);
        positionBuffer.GetDeviceAddress(device);
        vertexBuffer.GetDeviceAddress(device);

        Vk::SetDebugName(device, indexBuffer.handle,    "GeometryBuffer/IndexBuffer");
        Vk::SetDebugName(device, positionBuffer.handle, "GeometryBuffer/PositionBuffer");
        Vk::SetDebugName(device, vertexBuffer.handle,   "GeometryBuffer/VertexBuffer");
    }

    void GeometryBuffer::FlushUploads
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::Buffer& destination,
        const std::vector<Upload>& uploads,
        usize elementSize,
        VkPipelineStageFlags2 dstStageMask,
        VkAccessFlags2 dstAccessMask
    )
    {
        for (const auto& [info, stagingBuffer] : uploads)
        {
            UploadToBuffer
            (
                cmdBuffer,
                stagingBuffer,
                destination,
                info.offset * elementSize,
                info.count * elementSize,
                dstStageMask,
                dstAccessMask
            );
        }
    }

    void GeometryBuffer::UploadToBuffer
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::Buffer& source,
        const Vk::Buffer& destination,
        VkDeviceSize offsetBytes,
        VkDeviceSize sizeBytes,
        VkPipelineStageFlags2 dstStageMask,
        VkAccessFlags2 dstAccessMask
    )
    {
        if ((sizeBytes + offsetBytes) > destination.requestedSize)
        {
            Logger::Error
            (
                "Not enough space in destination buffer! [Required={}] [Available={}]\n",
                sizeBytes,
                destination.requestedSize - offsetBytes
            );
        }

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
            .srcBuffer   = source.handle,
            .dstBuffer   = destination.handle,
            .regionCount = 1,
            .pRegions    = &copyRegion
        };

        vkCmdCopyBuffer2(cmdBuffer.handle, &copyInfo);

        destination.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            dstStageMask,
            dstAccessMask,
            offsetBytes,
            sizeBytes
        );
    }

    void GeometryBuffer::ImGuiDisplay() const
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Geometry Buffer"))
            {
                ImGui::Text("Buffer Name     | Count  | Used/Available/Allocated");
                ImGui::Separator();

                ImGui::Text
                (
                    "Index Buffer    | %u | %llu/%llu/%llu",
                    indexCount,
                    indexCount * sizeof(Models::Index),
                    indexBuffer.allocationInfo.size - (indexCount * sizeof(Models::Index)),
                    indexBuffer.allocationInfo.size
                );

                ImGui::Text
                (
                    "Position Buffer | %u | %llu/%llu/%llu",
                    positionCount,
                    positionCount * sizeof(Models::Position),
                    positionBuffer.allocationInfo.size - (positionCount * sizeof(Models::Position)),
                    positionBuffer.allocationInfo.size
                );

                ImGui::Text
                (
                    "Vertex Buffer   | %u | %llu/%llu/%llu",
                    vertexCount,
                    vertexCount * sizeof(Models::Vertex),
                    vertexBuffer.allocationInfo.size - (vertexCount * sizeof(Models::Vertex)),
                    vertexBuffer.allocationInfo.size
                );

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }

    bool GeometryBuffer::HasPendingUploads() const
    {
        return !m_pendingIndexUploads.empty()  || !m_pendingPositionUploads.empty() ||
               !m_pendingVertexUploads.empty() || m_cubeStagingBuffer.has_value();
    }

    void GeometryBuffer::Destroy(VmaAllocator allocator)
    {
        indexBuffer.Destroy(allocator);
        positionBuffer.Destroy(allocator);
        vertexBuffer.Destroy(allocator);
        cubeBuffer.Destroy(allocator);
    }
}