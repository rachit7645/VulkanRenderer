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

#include "Util/Log.h"
#include "DebugUtils.h"
#include "Models/Model.h"
#include "GPU/Vertex.h"
#include "Renderer/RenderConstants.h"

namespace Vk
{
    GeometryBuffer::GeometryBuffer(const Vk::Context& context)
        : indexBuffer(context.extensions),
          positionBuffer(context.extensions),
          uvBuffer(context.extensions),
          vertexBuffer(context.extensions)
    {
        cubeBuffer = Vk::Buffer
        (
            context.allocator,
            36 * sizeof(GPU::Position),
            0,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        );

        cubeBuffer.GetDeviceAddress(context.device);

        SetupCubeUpload(context.allocator);

        Vk::SetDebugName(context.device, cubeBuffer.handle, "GeometryBuffer/CubeBuffer");
    }

    void GeometryBuffer::Bind(const Vk::CommandBuffer& cmdBuffer) const
    {
        indexBuffer.Bind(cmdBuffer);
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

        Vk::BeginLabel(cmdBuffer, "Index Transfer", {0.8901f, 0.0549f, 0.3607f, 1.0f});

        indexBuffer.FlushUploads
        (
            cmdBuffer,
            device,
            allocator,
            deletionQueue
        );

        Vk::EndLabel(cmdBuffer);

        Vk::BeginLabel(cmdBuffer, "Position Transfer", {0.4039f, 0.0509f, 0.5215f, 1.0f});

        positionBuffer.FlushUploads
        (
            cmdBuffer,
            device,
            allocator,
            deletionQueue
        );

        Vk::EndLabel(cmdBuffer);

        Vk::BeginLabel(cmdBuffer, "UV Transfer", {0.6117f, 0.0549f, 0.8901f, 1.0f});

        uvBuffer.FlushUploads
        (
            cmdBuffer,
            device,
            allocator,
            deletionQueue
        );

        Vk::EndLabel(cmdBuffer);

        Vk::BeginLabel(cmdBuffer, "Vertex Transfer", {0.6117f, 0.0549f, 0.8901f, 1.0f});

        vertexBuffer.FlushUploads
        (
            cmdBuffer,
            device,
            allocator,
            deletionQueue
        );

        Vk::EndLabel(cmdBuffer);

        if (m_pendingCubeUpload.has_value())
        {
            Vk::BeginLabel(cmdBuffer, "Cube Transfer", {0.5117f, 0.0749f, 0.3901f, 1.0f});

            constexpr VkDeviceSize VERTICES_SIZE = 36 * 3 * sizeof(f32);

            const VkBufferCopy2 copyRegion =
            {
                .sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                .pNext     = nullptr,
                .srcOffset = 0,
                .dstOffset = 0,
                .size      = VERTICES_SIZE
            };

            const VkCopyBufferInfo2 copyInfo =
            {
                .sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
                .pNext       = nullptr,
                .srcBuffer   = m_pendingCubeUpload->handle,
                .dstBuffer   = cubeBuffer.handle,
                .regionCount = 1,
                .pRegions    = &copyRegion
            };

            vkCmdCopyBuffer2(cmdBuffer.handle, &copyInfo);

            cubeBuffer.Barrier
            (
                cmdBuffer,
                Vk::BufferBarrier{
                    .srcStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                    .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .dstStageMask   = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                    .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                    .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .offset         = 0,
                    .size           = VERTICES_SIZE
                }
            );

            Vk::EndLabel(cmdBuffer);
        }

        Vk::EndLabel(cmdBuffer);

        Vk::SetDebugName(device, GetIndexBuffer().handle,    "GeometryBuffer/IndexBuffer");
        Vk::SetDebugName(device, GetPositionBuffer().handle, "GeometryBuffer/PositionBuffer");
        Vk::SetDebugName(device, GetUVBuffer().handle,       "GeometryBuffer/UVBuffer");
        Vk::SetDebugName(device, GetVertexBuffer().handle,   "GeometryBuffer/VertexBuffer");

        if (m_pendingCubeUpload.has_value())
        {
            deletionQueue.PushDeletor([allocator, buffer = m_pendingCubeUpload.value()] () mutable
            {
                buffer.Destroy(allocator);
            });

            m_pendingCubeUpload = std::nullopt;
        }
    }

    void GeometryBuffer::Free(const GPU::SurfaceInfo& info, Util::DeletionQueue& deletionQueue)
    {
        deletionQueue.PushDeletor([this, info] ()
        {
            indexBuffer.Free(info.indexInfo);
            positionBuffer.Free(info.positionInfo);
            uvBuffer.Free(info.uvInfo);
            vertexBuffer.Free(info.vertexInfo);
        });
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

        m_pendingCubeUpload = Vk::Buffer
        (
            allocator,
            VERTICES_SIZE,
            0,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            VMA_MEMORY_USAGE_AUTO
        );

        std::memcpy(m_pendingCubeUpload->hostAddress, CUBE_VERTICES.data(), VERTICES_SIZE);
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
                    indexBuffer.count,
                    indexBuffer.count * sizeof(GPU::Index),
                    GetIndexBuffer().size - (indexBuffer.count * sizeof(GPU::Index)),
                    GetIndexBuffer().size
                );

                ImGui::Text
                (
                    "Position Buffer | %u | %llu/%llu/%llu",
                    positionBuffer.count,
                    positionBuffer.count * sizeof(GPU::Position),
                    GetPositionBuffer().size - (positionBuffer.count * sizeof(GPU::Position)),
                    GetPositionBuffer().size
                );

                ImGui::Text
                (
                    "UV Buffer       | %u | %llu/%llu/%llu",
                    uvBuffer.count,
                    uvBuffer.count * sizeof(GPU::UV),
                    GetUVBuffer().size - (uvBuffer.count * sizeof(GPU::UV)),
                    GetUVBuffer().size
                );

                ImGui::Text
                (
                    "Vertex Buffer   | %u | %llu/%llu/%llu",
                    vertexBuffer.count,
                    vertexBuffer.count * sizeof(GPU::Vertex),
                    GetVertexBuffer().size - (vertexBuffer.count * sizeof(GPU::Vertex)),
                    GetVertexBuffer().size
                );

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }

    bool GeometryBuffer::HasPendingUploads() const
    {
        return indexBuffer.HasPendingUploads() || positionBuffer.HasPendingUploads() ||
               uvBuffer.HasPendingUploads() || vertexBuffer.HasPendingUploads() ||
               m_pendingCubeUpload.has_value();
    }

    const Vk::Buffer& GeometryBuffer::GetIndexBuffer() const
    {
        return indexBuffer.GetBuffer();
    }

    const Vk::Buffer& GeometryBuffer::GetPositionBuffer() const
    {
        return positionBuffer.GetBuffer();
    }

    const Vk::Buffer& GeometryBuffer::GetUVBuffer() const
    {
        return uvBuffer.GetBuffer();
    }

    const Vk::Buffer& GeometryBuffer::GetVertexBuffer() const
    {
        return vertexBuffer.GetBuffer();
    }

    void GeometryBuffer::Destroy(VmaAllocator allocator)
    {
        indexBuffer.Destroy(allocator);
        positionBuffer.Destroy(allocator);
        uvBuffer.Destroy(allocator);
        vertexBuffer.Destroy(allocator);
        cubeBuffer.Destroy(allocator);

        if (m_pendingCubeUpload.has_value())
        {
            m_pendingCubeUpload = std::nullopt;
        }
    }
}