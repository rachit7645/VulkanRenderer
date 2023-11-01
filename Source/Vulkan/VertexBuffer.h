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
            const VkPhysicalDeviceMemoryProperties& memProperties,
            const std::vector<Renderer::Vertex>& vertices
        );
        // Bind buffer
        void BindBuffer(VkCommandBuffer commandBuffer);
        // Destroys the buffer
        void DestroyBuffer(VkDevice device);
        // Buffer handle
        VkBuffer handle = {};
        // Buffer memory
        VkDeviceMemory memory = {};
        // Vertex count
        u32 vertexCount = 0;
    private:
        // Creates the buffer
        void CreateBuffer(VkDevice device);
        // Allocate memory
        void AllocateMemory(VkDevice device, const VkPhysicalDeviceMemoryProperties& memProperties);
        // Load data
        void LoadData(VkDevice device, const std::vector<Renderer::Vertex>& vertices);
        // Find memory type
        u32 FindMemoryType
        (
            u32 typeFilter,
            VkMemoryPropertyFlags properties,
            const VkPhysicalDeviceMemoryProperties& memProperties
        );
    };
}

#endif