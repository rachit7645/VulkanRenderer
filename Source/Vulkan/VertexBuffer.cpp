#include "VertexBuffer.h"
#include "Util/Log.h"

// Usings
using Renderer::Vertex;

namespace Vk
{
    VertexBuffer::VertexBuffer
    (
        VkDevice device,
        const VkPhysicalDeviceMemoryProperties& memProperties,
        const std::vector<Renderer::Vertex>& vertices
    )
        : vertexCount(static_cast<u32>(vertices.size()))
    {
        // Create buffer object
        CreateBuffer(device);
        // Allocate memory for buffer
        AllocateMemory(device, memProperties);
        // Load vertex data
        LoadData(device, vertices);
    }

    void VertexBuffer::CreateBuffer(VkDevice device)
    {
        // Creation info
        VkBufferCreateInfo createInfo =
        {
            .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .size                  = vertexCount * sizeof(Vertex),
            .usage                 = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices   = nullptr
        };

        // Create buffer
        if (vkCreateBuffer(
            device,
            &createInfo,
            nullptr,
            &handle
        ) != VK_SUCCESS)
        {
            // Log
            LOG_ERROR("Failed to create vertex buffer! [device={}]\n", reinterpret_cast<void*>(device));
        }

        // Log
        LOG_INFO("Created vertex buffer! [handle={}]\n", reinterpret_cast<void*>(handle));
    }

    void VertexBuffer::DestroyBuffer(VkDevice device)
    {
        // Log
        LOG_INFO
        (
            "Destroying buffer! [buffer={}] [memory={}]\n",
            reinterpret_cast<void*>(handle),
            reinterpret_cast<void*>(memory)
        );
        // Destroy
        vkDestroyBuffer(device, handle, nullptr);
        // Free
        vkFreeMemory(device, memory, nullptr);
    }

    void VertexBuffer::AllocateMemory(VkDevice device, const VkPhysicalDeviceMemoryProperties& memProperties)
    {
        // Buffer memory flags
        constexpr auto BUFFER_MEMORY_FLAGS = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        // Memory requirements
        VkMemoryRequirements memRequirements = {};
        vkGetBufferMemoryRequirements(device, handle, &memRequirements);

        // Allocation info
        VkMemoryAllocateInfo allocInfo =
        {
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext           = nullptr,
            .allocationSize  = memRequirements.size,
            .memoryTypeIndex = FindMemoryType(
                memRequirements.memoryTypeBits,
                BUFFER_MEMORY_FLAGS,
                memProperties
            )
        };

        // Allocate
        if (vkAllocateMemory(
                device,
                &allocInfo,
                nullptr,
                &memory
            ) != VK_SUCCESS)
        {
            // Log
            LOG_ERROR
            (
                "Failed to allocate vertex buffer memory! [device={}] [buffer={}]\n",
                reinterpret_cast<void*>(device),
                reinterpret_cast<void*>(handle)
            );
        }

        // Attach memory to buffer
        vkBindBufferMemory(device, handle, memory, 0);

        // Log
        LOG_INFO
        (
            "Allocated memory for buffer! [buffer={}] [size={}]\n",
            reinterpret_cast<void*>(handle),
            memRequirements.size
        );
    }

    void VertexBuffer::LoadData(VkDevice device, const std::vector<Renderer::Vertex>& vertices)
    {
        // Calculate size
        auto size = vertexCount * sizeof(Vertex);
        // Pointer to buffer
        void* mapPtr = nullptr;
        // Map
        vkMapMemory(device, memory, 0, size, 0, &mapPtr);
        // Copy
        std::memcpy(mapPtr, vertices.data(), size);
        // Unmap
        vkUnmapMemory(device, memory);
    }

    u32 VertexBuffer::FindMemoryType
    (
        u32 typeFilter,
        VkMemoryPropertyFlags properties,
        const VkPhysicalDeviceMemoryProperties& memProperties
    )
    {
        // Search through memory types
        for (u32 i = 0; i < memProperties.memoryTypeCount; ++i)
        {
            // Type check
            bool typeCheck = typeFilter & (0x1 << i);
            // Property check
            bool propertyCheck = (memProperties.memoryTypes[i].propertyFlags & properties) == properties;
            // Check
            if (typeCheck && propertyCheck)
            {
                // Found memory!
                return i;
            }
        }

        // If we didn't find memory, terminate
        LOG_ERROR("Failed to find suitable memory type! [buffer={}]\n", reinterpret_cast<void*>(handle));
    }

    void VertexBuffer::BindBuffer(VkCommandBuffer commandBuffer)
    {
        // Buffers to bind
        VkBuffer vertexBuffers[] = {handle};
        // Offsets
        VkDeviceSize offsets[] = {0};
        // Bind buffer
        vkCmdBindVertexBuffers
        (
            commandBuffer,
            0,
            1,
            vertexBuffers,
            offsets
        );
    }
}