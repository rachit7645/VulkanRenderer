#ifndef VK_CONTEXT_H
#define VK_CONTEXT_H

#include <memory>
#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include "ValidationLayers.h"
#include "QueueFamilyIndices.h"
#include "Extensions.h"

namespace Vk
{
    class Context
    {
    public:
        // Initialise vulkan context
        explicit Context(SDL_Window* window);
        // Destroy vulkan context
        ~Context();

        // Vulkan instance
        VkInstance vkInstance = {};
    private:
        // Create vulkan instance
        void CreateVKInstance(SDL_Window* window);
        // Create platform dependent surface
        void CreateSurface(SDL_Window* window);
        // Pick a GPU
        void PickPhysicalDevice();
        // Get queue families
        Vk::QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
        // Create a logical device
        void CreateLogicalDevice();

        // Extensions
        std::unique_ptr<Vk::Extensions> m_extensions = nullptr;
        #ifdef ENGINE_DEBUG
        // Vulkan validation layers
        std::unique_ptr<Vk::ValidationLayers> m_layers = nullptr;
        #endif
        // Surface
        VkSurfaceKHR m_surface = {};
        // Physical device (GPU)
        VkPhysicalDevice m_physicalDevice = {};
        // Queue families
        Vk::QueueFamilyIndices m_queueFamilies = {};
        // Logical device
        VkDevice m_logicalDevice = {};
        // Queue
        VkQueue m_graphicsQueue = {};
    };
}

#endif