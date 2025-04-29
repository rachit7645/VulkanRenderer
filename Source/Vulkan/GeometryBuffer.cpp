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
        const std::span<const Models::Vertex> vertices,
        Util::DeletionQueue& deletionQueue
    )
    {
        const auto indexInfo = SetupUpload
        (
            allocator,
            indices,
            indexCount,
            m_pendingIndexUploads,
            deletionQueue
        );

        const auto positionInfo = SetupUpload
        (
            allocator,
            positions,
            positionCount,
            m_pendingPositionUploads,
            deletionQueue
        );

        const auto vertexInfo = SetupUpload
        (
            allocator,
            vertices,
            vertexCount,
            m_pendingVertexUploads,
            deletionQueue
        );

        return {indexInfo, positionInfo, vertexInfo};
    }

    void GeometryBuffer::Update
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

        Vk::BeginLabel(cmdBuffer, "Geometry Transfer", {0.9882f, 0.7294f, 0.0118f, 1.0f});

        ResizeBuffers(cmdBuffer, device, allocator, deletionQueue);

        Vk::BeginLabel(cmdBuffer, "Index Transfer", {0.8901f, 0.0549f, 0.3607f, 1.0f});

        FlushUploads
        (
            cmdBuffer,
            indexBuffer,
            m_pendingIndexUploads,
            sizeof(Models::Index),
            VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT | VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_ACCESS_2_INDEX_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT
        );

        Vk::EndLabel(cmdBuffer);

        Vk::BeginLabel(cmdBuffer, "Position Transfer", {0.4039f, 0.0509f, 0.5215f, 1.0f});

        FlushUploads
        (
            cmdBuffer,
            positionBuffer,
            m_pendingPositionUploads,
            sizeof(Models::Position),
            VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_ACCESS_2_SHADER_READ_BIT
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

        if (m_pendingCubeUpload.has_value())
        {
            Vk::BeginLabel(cmdBuffer, "Cube Transfer", {0.5117f, 0.0749f, 0.3901f, 1.0f});

            FlushUploads
            (
                cmdBuffer,
                cubeBuffer,
                std::span(&m_pendingCubeUpload.value(), 1),
                sizeof(f32),
                VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                VK_ACCESS_2_SHADER_STORAGE_READ_BIT
            );

            Vk::EndLabel(cmdBuffer);
        }

        Vk::EndLabel(cmdBuffer);

        if (m_pendingCubeUpload.has_value())
        {
            deletionQueue.PushDeletor([allocator, buffer = m_pendingCubeUpload->buffer] () mutable
            {
                buffer.Destroy(allocator);
            });

            m_pendingCubeUpload = std::nullopt;
        }

        m_pendingIndexUploads.clear();
        m_pendingPositionUploads.clear();
        m_pendingVertexUploads.clear();
    }

    void GeometryBuffer::SetupCubeUpload(VmaAllocator allocator)
    {
        constexpr std::array CUBE_VERTICES =
        {
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f,  1.0f
        };

        constexpr VkDeviceSize VERTICES_SIZE = CUBE_VERTICES.size() * sizeof(f32);

        auto cubeStagingBuffer = Vk::Buffer
        (
            allocator,
            VERTICES_SIZE,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            VMA_MEMORY_USAGE_AUTO
        );

        std::memcpy(cubeStagingBuffer.allocationInfo.pMappedData, CUBE_VERTICES.data(), VERTICES_SIZE);

        m_pendingCubeUpload = GeometryBuffer::Upload
        {
            .info = GeometryBuffer::Info{
                .offset = 0,
                .count  = CUBE_VERTICES.size()
            },
            .buffer = cubeStagingBuffer
        };
    }

    template<typename T>
    GeometryBuffer::Info GeometryBuffer::SetupUpload
    (
        VmaAllocator allocator,
        const std::span<const T> data,
        u32& offset,
        std::vector<Upload>& uploads,
        Util::DeletionQueue& deletionQueue
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

        deletionQueue.PushDeletor([allocator, buffer = stagingBuffer] () mutable
        {
            buffer.Destroy(allocator);
        });

        return info;
    }

    void GeometryBuffer::ResizeBuffer
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
    )
    {
        if (count == 0)
        {
            return;
        }

        if (count * elementSize > buffer.requestedSize)
        {
            u32 pendingCount = 0;

            for (const auto& upload : uploads)
            {
                pendingCount += upload.info.count;
            }

            if (pendingCount > count)
            {
                Logger::Error("{}\n", "Pending count exceeds total count!");
            }

            const u32          oldCount = count - pendingCount;
            const VkDeviceSize oldSize  = oldCount * elementSize;

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

            buffer.GetDeviceAddress(device);

            if (oldBuffer.handle != VK_NULL_HANDLE && oldCount > 0)
            {
                oldBuffer.Barrier
                (
                    cmdBuffer,
                    srcStageMask,
                    srcAccessMask,
                    VK_PIPELINE_STAGE_2_COPY_BIT,
                    VK_ACCESS_2_TRANSFER_READ_BIT,
                    0,
                    oldSize
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

    void GeometryBuffer::ResizeBuffers
    (
        const Vk::CommandBuffer& cmdBuffer,
        VkDevice device,
        VmaAllocator allocator,
        Util::DeletionQueue& deletionQueue
    )
    {
        ResizeBuffer
        (
            cmdBuffer,
            device,
            allocator,
            indexCount,
            sizeof(Models::Index),
            m_pendingIndexUploads,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
            VK_ACCESS_2_INDEX_READ_BIT,
            indexBuffer,
            deletionQueue
        );

        ResizeBuffer
        (
            cmdBuffer,
            device,
            allocator,
            positionCount,
            sizeof(Models::Position),
            m_pendingPositionUploads,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
            positionBuffer,
            deletionQueue
        );

        ResizeBuffer
        (
            cmdBuffer,
            device,
            allocator,
            vertexCount,
            sizeof(Models::Vertex),
            m_pendingVertexUploads,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
            vertexBuffer,
            deletionQueue
        );

        Vk::SetDebugName(device, indexBuffer.handle,    "GeometryBuffer/IndexBuffer");
        Vk::SetDebugName(device, positionBuffer.handle, "GeometryBuffer/PositionBuffer");
        Vk::SetDebugName(device, vertexBuffer.handle,   "GeometryBuffer/VertexBuffer");
    }

    void GeometryBuffer::FlushUploads
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::Buffer& destination,
        const std::span<const Upload> uploads,
        usize elementSize,
        VkPipelineStageFlags2 dstStageMask,
        VkAccessFlags2 dstAccessMask
    )
    {
        // Copy
        {
            for (const auto& [info, stagingBuffer] : uploads)
            {
                const VkBufferCopy2 copyRegion =
                {
                    .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                    .pNext     = nullptr,
                    .srcOffset = 0,
                    .dstOffset = info.offset * elementSize,
                    .size      = info.count  * elementSize
                };

                const VkCopyBufferInfo2 copyInfo =
                {
                    .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
                    .pNext       = nullptr,
                    .srcBuffer   = stagingBuffer.handle,
                    .dstBuffer   = destination.handle,
                    .regionCount = 1,
                    .pRegions    = &copyRegion
                };

                vkCmdCopyBuffer2(cmdBuffer.handle, &copyInfo);
            }
        }

        // Barriers
        {
            std::vector<VkBufferMemoryBarrier2> copyToDstBarriers = {};
            copyToDstBarriers.reserve(uploads.size());

            for (const auto& [info, stagingBuffer] : uploads)
            {
                copyToDstBarriers.emplace_back(VkBufferMemoryBarrier2{
                    .sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                    .pNext               = nullptr,
                    .srcStageMask        = VK_PIPELINE_STAGE_2_COPY_BIT,
                    .srcAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .dstStageMask        = dstStageMask,
                    .dstAccessMask       = dstAccessMask,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .buffer              = destination.handle,
                    .offset              = info.offset * elementSize,
                    .size                = info.count  * elementSize
                });
            }

            const VkDependencyInfo dependencyInfo =
            {
                .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .pNext                    = nullptr,
                .dependencyFlags          = 0,
                .memoryBarrierCount       = 0,
                .pMemoryBarriers          = nullptr,
                .bufferMemoryBarrierCount = static_cast<u32>(copyToDstBarriers.size()),
                .pBufferMemoryBarriers    = copyToDstBarriers.data(),
                .imageMemoryBarrierCount  = 0,
                .pImageMemoryBarriers     = nullptr
            };

            vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencyInfo);
        }
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
               !m_pendingVertexUploads.empty() || m_pendingCubeUpload.has_value();
    }

    void GeometryBuffer::Destroy(VmaAllocator allocator)
    {
        indexBuffer.Destroy(allocator);
        positionBuffer.Destroy(allocator);
        vertexBuffer.Destroy(allocator);
        cubeBuffer.Destroy(allocator);

        if (m_pendingCubeUpload.has_value())
        {
            m_pendingCubeUpload = std::nullopt;
        }

        m_pendingIndexUploads.clear();
        m_pendingPositionUploads.clear();
        m_pendingVertexUploads.clear();
    }
}