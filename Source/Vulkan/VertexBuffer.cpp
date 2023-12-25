/*
 *    Copyright 2023 Rachit Khandelwal
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "VertexBuffer.h"
#include "Util/Log.h"

// Usings
using Models::Vertex;
using Models::Index;

namespace Vk
{
    VertexBuffer::VertexBuffer
    (
        const std::shared_ptr<Vk::Context>& context,
        const std::span<const Vertex> vertices,
        const std::span<const Index> indices
    )
        : vertexCount(static_cast<u32>(vertices.size())),
          indexCount(static_cast<u32>(indices.size()))
    {
        // Initialise buffers
        InitBuffer(context, vertexBuffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertices);
        InitBuffer(context, indexBuffer,  VK_BUFFER_USAGE_INDEX_BUFFER_BIT,   indices);
    }

    VertexBuffer::VertexBuffer(const std::shared_ptr<Vk::Context>& context, const std::span<const f32> vertices)
        : vertexCount(vertices.size())
    {
        // Init vertex buffer
        InitBuffer(context, vertexBuffer, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertices);
    }

    void VertexBuffer::BindBuffer(const Vk::CommandBuffer& cmdBuffer) const
    {
        // Buffers to bind
        std::array<VkBuffer, 1> vertexBuffers = {vertexBuffer.handle};
        // Offsets
        std::array<VkDeviceSize, 1> offsets = {0};

        // Bind vertex buffers
        vkCmdBindVertexBuffers
        (
            cmdBuffer.handle,
            0,
            static_cast<u32>(vertexBuffers.size()),
            vertexBuffers.data(),
            offsets.data()
        );

        // Bind index buffer
        if (indexCount > 0)
        {
            vkCmdBindIndexBuffer(cmdBuffer.handle, indexBuffer.handle, 0, indexType);
        }
    }

    void VertexBuffer::DestroyBuffer(VkDevice device) const
    {
        // Delete buffers
        vertexBuffer.DeleteBuffer(device);
        indexBuffer.DeleteBuffer(device);
    }

    template <typename T>
    void VertexBuffer::InitBuffer
    (
        const std::shared_ptr<Vk::Context>& context,
        Vk::Buffer& buffer,
        VkBufferUsageFlags usage,
        const std::span<const T> data
    )
    {
        // Size of vertex data (bytes)
        VkDeviceSize size = data.size() * sizeof(T);

        // Create staging buffer
        auto stagingBuffer = Vk::Buffer
        (
            context,
            size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );

        // Load data
        stagingBuffer.LoadData(context->device, data);

        // Create real buffer
        buffer = Vk::Buffer
        (
            context,
            size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        // Copy
        Vk::Buffer::CopyBuffer(context, stagingBuffer, buffer, size);

        // Delete staging buffer
        stagingBuffer.DeleteBuffer(context->device);
    }
}