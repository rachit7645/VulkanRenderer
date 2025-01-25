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

        Vk::SetDebugName(device, indexBuffer.handle,     "GeometryBuffer/IndexBuffer");
        Vk::SetDebugName(device, vertexBuffer.handle,    "GeometryBuffer/VertexBuffer");
    }

    void GeometryBuffer::Bind(const Vk::CommandBuffer& cmdBuffer) const
    {
        vkCmdBindIndexBuffer(cmdBuffer.handle, indexBuffer.handle, 0, indexType);
    }

    std::pair<Models::Info, Models::Info> GeometryBuffer::SetupLoad
    (
        std::vector<Models::Index>&& indices,
        std::vector<Models::Vertex>&& vertices
    )
    {
        const Models::Info indexInfo =
        {
            .offset = indexCount,
            .count  = static_cast<u32>(indices.size())
        };

        const Models::Info vertexInfo =
        {
            .offset = vertexCount,
            .count  = static_cast<u32>(vertices.size())
        };

        indexCount  += indexInfo.count;
        vertexCount += vertexInfo.count;

        m_indicesToLoad.push_back(std::make_pair(indexInfo, std::move(indices)));
        m_verticesToLoad.push_back(std::make_pair(vertexInfo, std::move(vertices)));

        return {indexInfo, vertexInfo};
    }

    void GeometryBuffer::Update(const Vk::Context& context)
    {
        auto cmdBuffer = Vk::CommandBuffer
        (
            context.device,
            context.commandPool,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY
        );

        std::vector<Vk::Buffer> stagingBuffers = {};

        Vk::BeginLabel(context.graphicsQueue, "Geometry Transfer", {0.9882f, 0.7294f, 0.0118f, 1.0f});

        cmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        Vk::BeginLabel(cmdBuffer, "Index Transfer", {0.8901f, 0.0549f, 0.3607f, 1.0f});
        for (usize i = 0; i < m_indicesToLoad.size(); ++i)
        {
            const auto& [indexInfo, indices] = m_indicesToLoad[i];

            const VkDeviceSize offsetBytes = indexInfo.offset * sizeof(Models::Index);
            const VkDeviceSize sizeBytes   = indexInfo.count  * sizeof(Models::Index);

            stagingBuffers.emplace_back
            (
                context.allocator,
                sizeBytes,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            const auto& stagingBuffer = stagingBuffers[i];

            UploadToBuffer
            (
                cmdBuffer,
                stagingBuffer,
                indexBuffer,
                offsetBytes,
                sizeBytes,
                indices.data()
            );
        }
        Vk::EndLabel(cmdBuffer);

        Vk::BeginLabel(cmdBuffer, "Vertex Transfer", {0.6117f, 0.0549f, 0.8901f, 1.0f});
        for (usize i = 0; i < m_verticesToLoad.size(); ++i)
        {
            const auto& [vertexInfo, vertices] = m_verticesToLoad[i];

            const VkDeviceSize offsetBytes = vertexInfo.offset * sizeof(Models::Vertex);
            const VkDeviceSize sizeBytes   = vertexInfo.count  * sizeof(Models::Vertex);

            stagingBuffers.emplace_back
            (
                context.allocator,
                sizeBytes,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            const auto& stagingBuffer = stagingBuffers[m_indicesToLoad.size() + i];

            UploadToBuffer
            (
                cmdBuffer,
                stagingBuffer,
                vertexBuffer,
                offsetBytes,
                sizeBytes,
                vertices.data()
            );
        }
        Vk::EndLabel(cmdBuffer);

        cmdBuffer.EndRecording();

        VkFence transferFence = VK_NULL_HANDLE;

        // Submit
        {
            const VkFenceCreateInfo fenceCreateInfo =
            {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0
            };

            vkCreateFence(context.device, &fenceCreateInfo, nullptr, &transferFence);

            VkCommandBufferSubmitInfo cmdBufferInfo =
            {
                .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .pNext         = nullptr,
                .commandBuffer = cmdBuffer.handle,
                .deviceMask    = 0
            };

            const VkSubmitInfo2 submitInfo =
            {
                .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .pNext                    = nullptr,
                .flags                    = 0,
                .waitSemaphoreInfoCount   = 0,
                .pWaitSemaphoreInfos      = nullptr,
                .commandBufferInfoCount   = 1,
                .pCommandBufferInfos      = &cmdBufferInfo,
                .signalSemaphoreInfoCount = 0,
                .pSignalSemaphoreInfos    = nullptr
            };

            Vk::CheckResult(vkQueueSubmit2(
                context.graphicsQueue,
                1,
                &submitInfo,
                transferFence),
                "Failed to submit tranfer command buffers!"
            );

            Vk::CheckResult(vkWaitForFences(
                context.device,
                1,
                &transferFence,
                VK_TRUE,
                std::numeric_limits<u64>::max()),
                "Error while waiting for tranfers!"
            );
        }

        Vk::EndLabel(context.graphicsQueue);

        // Cleanup
        {
            vkDestroyFence(context.device, transferFence, nullptr);

            for (const auto& buffer : stagingBuffers)
            {
                buffer.Destroy(context.allocator);
            }

            cmdBuffer.Free(context.device, context.commandPool);

            m_indicesToLoad.clear();
            m_verticesToLoad.clear();
        }
    }

    void GeometryBuffer::UploadToBuffer
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::Buffer& source,
        const Vk::Buffer& destination,
        VkDeviceSize offsetBytes,
        VkDeviceSize sizeBytes,
        const void* data
    )
    {
        CheckSize(sizeBytes, 0,           source.allocInfo.size);
        CheckSize(sizeBytes, offsetBytes, destination.allocInfo.size);

        std::memcpy(source.allocInfo.pMappedData, data, sizeBytes);

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
            VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
            VK_ACCESS_2_INDEX_READ_BIT,
            offsetBytes,
            sizeBytes
        );
    }

    void GeometryBuffer::CheckSize(usize sizeBytes, usize offsetBytes, usize bufferSize)
    {
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
    }
}