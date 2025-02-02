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

#include <volk/volk.h>

#include "Util/Log.h"
#include "Models/Model.h"
#include "DebugUtils.h"

namespace Vk
{
    GeometryBuffer::GeometryBuffer(VkDevice device, VmaAllocator allocator)
    {
        // TODO: Implement resizing
        constexpr VkDeviceSize INITIAL_INDEX_SIZE    = (1 << 20) * sizeof(Models::Index);
        constexpr VkDeviceSize INITIAL_POSITION_SIZE = (1 << 18) * sizeof(glm::vec3);
        constexpr VkDeviceSize INITIAL_VERTEX_SIZE   = (1 << 18) * sizeof(Models::Vertex);

        indexBuffer = Vk::Buffer
        (
            allocator,
            INITIAL_INDEX_SIZE,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        );

        positionBuffer = Vk::Buffer
        (
            allocator,
            INITIAL_POSITION_SIZE,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        );

        vertexBuffer = Vk::Buffer
        (
            allocator,
            INITIAL_VERTEX_SIZE,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        );

        positionBuffer.GetDeviceAddress(device);
        vertexBuffer.GetDeviceAddress(device);

        Vk::SetDebugName(device, indexBuffer.handle,    "GeometryBuffer/IndexBuffer");
        Vk::SetDebugName(device, positionBuffer.handle, "GeometryBuffer/PositionBuffer");
        Vk::SetDebugName(device, vertexBuffer.handle,   "GeometryBuffer/VertexBuffer");
    }

    void GeometryBuffer::Bind(const Vk::CommandBuffer& cmdBuffer) const
    {
        vkCmdBindIndexBuffer(cmdBuffer.handle, indexBuffer.handle, 0, VK_INDEX_TYPE_UINT32);
    }

    std::array<Models::Info, 3> GeometryBuffer::SetupUpload
    (
        VmaAllocator allocator,
        const std::vector<Models::Index>& indices,
        const std::vector<glm::vec3>& positions,
        const std::vector<Models::Vertex>& vertices
    )
    {
        auto SetupStagingBuffer = [allocator] (VkDeviceSize sizeBytes, const void* data) -> Vk::Buffer
        {
            const auto stagingBuffer = Vk::Buffer
            (
                allocator,
                sizeBytes,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            std::memcpy(stagingBuffer.allocInfo.pMappedData, data, sizeBytes);

            return stagingBuffer;
        };

        const Models::Info indexInfo =
        {
            .offset = indexCount,
            .count  = static_cast<u32>(indices.size())
        };

        const Models::Info positionInfo =
        {
            .offset = positionCount,
            .count  = static_cast<u32>(positions.size())
        };

        const Models::Info vertexInfo =
        {
            .offset = vertexCount,
            .count  = static_cast<u32>(vertices.size())
        };

        const VkDeviceSize indexSizeBytes    = indexInfo.count    * sizeof(Models::Index);
        const VkDeviceSize positionSizeBytes = positionInfo.count * sizeof(glm::vec3);
        const VkDeviceSize vertexSizeBytes   = vertexInfo.count   * sizeof(Models::Vertex);

        m_pendingIndexUploads.push_back(std::make_pair(indexInfo, SetupStagingBuffer(indexSizeBytes, indices.data())));
        m_pendingPositionUploads.push_back(std::make_pair(positionInfo, SetupStagingBuffer(positionSizeBytes, positions.data())));
        m_pendingVertexUploads.push_back(std::make_pair(vertexInfo, SetupStagingBuffer(vertexSizeBytes, vertices.data())));

        indexCount    += indices.size();
        positionCount += positions.size();
        vertexCount   += vertices.size();

        return {indexInfo, positionInfo, vertexInfo};
    }

    void GeometryBuffer::Update(const Vk::CommandBuffer& cmdBuffer)
    {
        Vk::BeginLabel(cmdBuffer, "Geometry Transfer", {0.9882f, 0.7294f, 0.0118f, 1.0f});

        Vk::BeginLabel(cmdBuffer, "Index Transfer", {0.8901f, 0.0549f, 0.3607f, 1.0f});
        for (const auto& [indexInfo, indexStagingBuffer] : m_pendingIndexUploads)
        {
            const VkDeviceSize offsetBytes = indexInfo.offset * sizeof(Models::Index);
            const VkDeviceSize sizeBytes   = indexInfo.count  * sizeof(Models::Index);

            UploadToBuffer
            (
                cmdBuffer,
                indexStagingBuffer,
                indexBuffer,
                offsetBytes,
                sizeBytes,
                VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
                VK_ACCESS_2_INDEX_READ_BIT
            );
        }
        Vk::EndLabel(cmdBuffer);

        Vk::BeginLabel(cmdBuffer, "Position Transfer", {0.4039f, 0.0509f, 0.5215f, 1.0f});
        for (const auto& [positionInfo, positionStagingBuffer] : m_pendingPositionUploads)
        {
            const VkDeviceSize offsetBytes = positionInfo.offset * sizeof(glm::vec3);
            const VkDeviceSize sizeBytes   = positionInfo.count  * sizeof(glm::vec3);

            UploadToBuffer
            (
                cmdBuffer,
                positionStagingBuffer,
                positionBuffer,
                offsetBytes,
                sizeBytes,
                VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                VK_ACCESS_2_SHADER_STORAGE_READ_BIT
            );
        }
        Vk::EndLabel(cmdBuffer);

        Vk::BeginLabel(cmdBuffer, "Vertex Transfer", {0.6117f, 0.0549f, 0.8901f, 1.0f});
        for (const auto& [vertexInfo, vertexStagingBuffer] : m_pendingVertexUploads)
        {
            const VkDeviceSize offsetBytes = vertexInfo.offset * sizeof(Models::Vertex);
            const VkDeviceSize sizeBytes   = vertexInfo.count  * sizeof(Models::Vertex);

            UploadToBuffer
            (
                cmdBuffer,
                vertexStagingBuffer,
                vertexBuffer,
                offsetBytes,
                sizeBytes,
                VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                VK_ACCESS_2_SHADER_STORAGE_READ_BIT
            );
        }
        Vk::EndLabel(cmdBuffer);

        Vk::EndLabel(cmdBuffer);
    }

    void GeometryBuffer::Clear(VmaAllocator allocator)
    {
        for (const auto& buffer : m_pendingIndexUploads | std::views::values)
        {
            buffer.Destroy(allocator);
        }

        for (const auto& buffer : m_pendingPositionUploads | std::views::values)
        {
            buffer.Destroy(allocator);
        }

        for (const auto& buffer : m_pendingVertexUploads | std::views::values)
        {
            buffer.Destroy(allocator);
        }

        m_pendingIndexUploads.clear();
        m_pendingPositionUploads.clear();
        m_pendingVertexUploads.clear();
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
        if ((sizeBytes + offsetBytes) > destination.allocInfo.size)
        {
            Logger::Error
            (
                "Not enough space in destination buffer! [Required={}] [Available={}]\n",
                sizeBytes,
                destination.allocInfo.size - offsetBytes
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
                    indexBuffer.allocInfo.size - (indexCount * sizeof(Models::Index)),
                    indexBuffer.allocInfo.size
                );

                ImGui::Text
                (
                    "Position Buffer | %u | %llu/%llu/%llu",
                    positionCount,
                    positionCount * sizeof(glm::vec3),
                    positionBuffer.allocInfo.size - (positionCount * sizeof(glm::vec3)),
                    positionBuffer.allocInfo.size
                );

                ImGui::Text
                (
                    "Vertex Buffer   | %u | %llu/%llu/%llu",
                    vertexCount,
                    vertexCount * sizeof(Models::Vertex),
                    vertexBuffer.allocInfo.size - (vertexCount * sizeof(Models::Vertex)),
                    vertexBuffer.allocInfo.size
                );

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }

    void GeometryBuffer::Destroy(VmaAllocator allocator) const
    {
        indexBuffer.Destroy(allocator);
        positionBuffer.Destroy(allocator);
        vertexBuffer.Destroy(allocator);
    }
}