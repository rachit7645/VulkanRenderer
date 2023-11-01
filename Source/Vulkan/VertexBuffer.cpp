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
            vertexHandle,
            vertexMemory,
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
            indexHandle,
            indexMemory,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            memProperties,
            indices
        );
    }

    void VertexBuffer::BindBuffer(VkCommandBuffer commandBuffer)
    {
        // Buffers to bind
        VkBuffer vertexBuffers[] = {vertexHandle};
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
        vkCmdBindIndexBuffer(commandBuffer, indexHandle, 0, indexType);
    }

    void VertexBuffer::DestroyBuffer(VkDevice device)
    {
        // Delete vertex buffer
        DeleteBuffer(device, vertexHandle, vertexMemory);
        // Delete index buffer
        DeleteBuffer(device, indexHandle, indexMemory);
    }

    template <typename T>
    void VertexBuffer::InitBuffer
    (
        VkDevice device,
        VkCommandPool commandPool,
        VkQueue queue,
        VkBuffer& buffer,
        VkDeviceMemory& bufferMemory,
        VkBufferUsageFlags usage,
        const VkPhysicalDeviceMemoryProperties& memProperties,
        const std::span<const T> data
    )
    {
        // Size of vertex data (bytes)
        VkDeviceSize size = data.size() * sizeof(T);
        // Staging buffer data
        VkBuffer       stagingBuffer       = {};
        VkDeviceMemory stagingBufferMemory = {};

        // Create staging buffer
        CreateBuffer
        (
            device,
            size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            memProperties,
            stagingBuffer,
            stagingBufferMemory
        );

        // Pointer to buffer
        void* mapPtr = nullptr;
        // Map
        vkMapMemory(device, stagingBufferMemory, 0, size, 0, &mapPtr);
        // Copy
        std::memcpy(mapPtr, data.data(), size);
        // Unmap
        vkUnmapMemory(device, stagingBufferMemory);

        // Create buffer
        CreateBuffer
        (
            device,
            size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            memProperties,
            buffer,
            bufferMemory
        );

        // Copy
        CopyBuffer(stagingBuffer, buffer, size, device, commandPool, queue);

        // Delete staging buffer
        DeleteBuffer(device, stagingBuffer, stagingBufferMemory);
    }

    void VertexBuffer::CreateBuffer
    (
        VkDevice device,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        const VkPhysicalDeviceMemoryProperties& memProperties,
        VkBuffer& outBuffer,
        VkDeviceMemory& outBufferMemory
    )
    {
        // Creation info
        VkBufferCreateInfo createInfo =
        {
            .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .size                  = size,
            .usage                 = usage,
            .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices   = nullptr
        };

        // Create buffer
        if (vkCreateBuffer(
            device,
            &createInfo,
            nullptr,
            &outBuffer
        ) != VK_SUCCESS)
        {
            // Log
            LOG_ERROR("Failed to create vertex buffer! [device={}]\n", reinterpret_cast<void*>(device));
        }

        // Log
        LOG_INFO("Created vertex buffer! [handle={}]\n", reinterpret_cast<void*>(outBuffer));

        // Memory requirements
        VkMemoryRequirements memRequirements = {};
        vkGetBufferMemoryRequirements(device, outBuffer, &memRequirements);

        // Allocation info
        VkMemoryAllocateInfo allocInfo =
        {
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext           = nullptr,
            .allocationSize  = memRequirements.size,
            .memoryTypeIndex = FindMemoryType(
                memRequirements.memoryTypeBits,
                properties,
                memProperties
            )
        };

        // Allocate
        if (vkAllocateMemory(
                device,
                &allocInfo,
                nullptr,
                &outBufferMemory
            ) != VK_SUCCESS)
        {
            // Log
            LOG_ERROR
            (
                "Failed to allocate vertex buffer memory! [device={}] [buffer={}]\n",
                reinterpret_cast<void*>(device),
                reinterpret_cast<void*>(outBuffer)
            );
        }

        // Attach memory to buffer
        vkBindBufferMemory(device, outBuffer, outBufferMemory, 0);

        // Log
        LOG_INFO
        (
            "Allocated memory for buffer! [buffer={}] [size={}]\n",
            reinterpret_cast<void*>(outBuffer),
            memRequirements.size
        );
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
        LOG_ERROR("Failed to find suitable memory type! [buffer={}]\n", reinterpret_cast<void*>(vertexHandle));
    }

    void VertexBuffer::CopyBuffer
    (
        VkBuffer srcBuffer,
        VkBuffer dstBuffer,
        VkDeviceSize size,
        VkDevice device,
        VkCommandPool commandPool,
        VkQueue queue
    )
    {
        // Transfer buffer
        VkCommandBufferAllocateInfo allocInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext              = nullptr,
            .commandPool        = commandPool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        // Create buffer
        VkCommandBuffer transferBuffer = {};
        vkAllocateCommandBuffers(device, &allocInfo, &transferBuffer);

        // Begin info
        VkCommandBufferBeginInfo beginInfo =
        {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext            = nullptr,
            .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr
        };

        // Begin
        vkBeginCommandBuffer(transferBuffer, &beginInfo);

        // Copy region
        VkBufferCopy copyRegion =
        {
            .srcOffset = 0,
            .dstOffset = 0,
            .size      = size
        };

        // Copy
        vkCmdCopyBuffer(transferBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
        // End
        vkEndCommandBuffer(transferBuffer);

        // Submit info
        VkSubmitInfo submitInfo =
        {
            .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext                = nullptr,
            .waitSemaphoreCount   = 0,
            .pWaitSemaphores      = nullptr,
            .pWaitDstStageMask    = nullptr,
            .commandBufferCount   = 1,
            .pCommandBuffers      = &transferBuffer,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores    = nullptr
        };

        // Submit
        vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue);

        // Cleanup
        vkFreeCommandBuffers(device, commandPool, 1, &transferBuffer);
    }

    void VertexBuffer::DeleteBuffer(VkDevice device, VkBuffer buffer, VkDeviceMemory bufferMemory)
    {
        // Log
        LOG_INFO
        (
            "Destroying buffer [buffer={}] [memory={}]\n",
            reinterpret_cast<void*>(buffer),
            reinterpret_cast<void*>(bufferMemory)
        );
        // Destroy
        vkDestroyBuffer(device, buffer, nullptr);
        // Free
        vkFreeMemory(device, bufferMemory, nullptr);
    }
}