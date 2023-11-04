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
using Renderer::Vertex;
using Renderer::Index;

namespace Vk
{
    VertexBuffer::VertexBuffer
    (
        VkDevice device,
        VkCommandPool commandPool,
        VkQueue queue,
        const VkPhysicalDeviceMemoryProperties& memProperties,
        const std::span<const Vertex> vertices,
        const std::span<const Index> indices
    )
        : vertexCount(static_cast<u32>(vertices.size())),
          indexCount(static_cast<u32>(indices.size()))
    {
        // Initialise vertex buffer
        InitBuffer
        (
            device,
            commandPool,
            queue,
            vertexBuffer,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            memProperties,
            vertices
        );

        // Initialise index buffer
        InitBuffer
        (
            device,
            commandPool,
            queue,
            indexBuffer,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            memProperties,
            indices
        );
    }

    void VertexBuffer::BindBuffer(VkCommandBuffer commandBuffer)
    {
        // Buffers to bind
        VkBuffer vertexBuffers[] = {vertexBuffer.handle};
        // Offsets
        VkDeviceSize offsets[] = {0};

        // Bind vertex buffers
        vkCmdBindVertexBuffers
        (
            commandBuffer,
            0,
            1,
            vertexBuffers,
            offsets
        );

        // Bind index buffer
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer.handle, 0, indexType);
    }

    void VertexBuffer::DestroyBuffer(VkDevice device)
    {
        // Delete buffers
        vertexBuffer.DeleteBuffer(device);
        indexBuffer.DeleteBuffer(device);
    }

    template <typename T>
    void VertexBuffer::InitBuffer
    (
        VkDevice device,
        VkCommandPool commandPool,
        VkQueue queue,
        Vk::Buffer& buffer,
        VkBufferUsageFlags usage,
        const VkPhysicalDeviceMemoryProperties& memProperties,
        const std::span<const T> data
    )
    {
        // Size of vertex data (bytes)
        VkDeviceSize size = data.size() * sizeof(T);

        // Create staging buffer
        auto stagingBuffer = Vk::Buffer
        (
            device,
            size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            memProperties
        );

        // Load data
        stagingBuffer.LoadData(device, data);

        // Create real buffer
        buffer = Vk::Buffer
        (
            device,
            size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            memProperties
        );

        // Copy
        Vk::Buffer::CopyBuffer(stagingBuffer, buffer, size, device, commandPool, queue);

        // Delete staging buffer
        stagingBuffer.DeleteBuffer(device);
    }
}