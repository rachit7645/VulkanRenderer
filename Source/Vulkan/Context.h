#ifndef VK_CONTEXT_H
#define VK_CONTEXT_H

#include <memory>
#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include "ValidationLayers.h"
#include "QueueFamilyIndex.h"

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
        // Pick a GPU
        void PickPhysicalDevice();
        // Get queue families
        static Vk::QueueFamilyIndex FindQueueFamilies(VkPhysicalDevice device);
        // Create a logical device
        void CreateLogicalDevice();

        // Vulkan validation layers
        std::unique_ptr<Vk::ValidationLayers> m_layers = nullptr;
        // Physical device (GPU)
        VkPhysicalDevice m_physicalDevice = {};
        // Queue families
        Vk::QueueFamilyIndex m_queueFamilies = {};
        // Logical device
        VkDevice m_logicalDevice = {};
    };
}

#endif