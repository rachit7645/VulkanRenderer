#ifndef VERTEX_BUFFER_H
#define VERTEX_BUFFER_H

#include <span>
#include <vulkan/vulkan.h>
#include "Renderer/Vertex.h"
#include "Util/Util.h"

namespace Vk
{
    class VertexBuffer
    {
    public:
        // Creates a vertex buffer
        VertexBuffer
        (
            VkDevice device,
            VkCommandPool commandPool,
            VkQueue queue,
            const VkPhysicalDeviceMemoryProperties& memProperties,
            const std::span<const Renderer::Vertex> vertices,
            const std::span<const Renderer::Index> indices
        );
        // Bind buffer
        void BindBuffer(VkCommandBuffer commandBuffer);
        // Destroys the buffer
        void DestroyBuffer(VkDevice device);

        // Vertex buffer handle
        VkBuffer vertexHandle = {};
        // Vertex buffer memory
        VkDeviceMemory vertexMemory = {};

        // Vertex buffer handle
        VkBuffer indexHandle = {};
        // Vertex buffer memory
        VkDeviceMemory indexMemory = {};

        // Vertex count
        UNUSED u32 vertexCount = 0;
        // Index count
        u32 indexCount = 0;
        // Index type
        VkIndexType indexType = std::is_same_v<Renderer::Index, u32> ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16;
    private:
        // Init buffer
        template<typename T>
        void InitBuffer
        (
            VkDevice device,
            VkCommandPool commandPool,
            VkQueue queue,
            VkBuffer& buffer,
            VkDeviceMemory& bufferMemory,
            VkBufferUsageFlags usage,
            const VkPhysicalDeviceMemoryProperties& memProperties,
            const std::span<const T> data
        );

        // Creates the buffer
        void CreateBuffer
        (
            VkDevice device,
            VkDeviceSize size,
            VkBufferUsageFlags usage,
            VkMemoryPropertyFlags properties,
            const VkPhysicalDeviceMemoryProperties& memProperties,
            VkBuffer& outBuffer,
            VkDeviceMemory& outBufferMemory
        );

        // Copy from src to dst buffer
        void CopyBuffer
        (
            VkBuffer srcBuffer,
            VkBuffer dstBuffer,
            VkDeviceSize size,
            VkDevice device,
            VkCommandPool commandPool,
            VkQueue queue
        );

        // Find memory type
        u32 FindMemoryType
        (
            u32 typeFilter,
            VkMemoryPropertyFlags properties,
            const VkPhysicalDeviceMemoryProperties& memProperties
        );

        // Internal deletion function
        void DeleteBuffer(VkDevice device, VkBuffer buffer, VkDeviceMemory bufferMemory);
    };
}

#endif