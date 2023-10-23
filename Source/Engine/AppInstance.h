#ifndef APP_INSTANCE_H
#define APP_INSTANCE_H

#include <vector>
#include <vulkan/vulkan.h>

#include "Window.h"
#include "../Util/Util.h"
#include "../Vulkan/ValidationLayers.h"
#include "../Vulkan/QueueFamilyIndex.h"

namespace Engine
{
    class AppInstance
    {
    public:
        // Create an instance of the application
        AppInstance();
        // Destroy application instance
        ~AppInstance();

        // Run application
        void Run();

    private:
        // Window
        std::unique_ptr<Engine::Window> m_window = nullptr;
        // Vulkan instance
        VkInstance m_vkInstance = {};
        // Vulkan validation layers
        std::unique_ptr<Vk::ValidationLayers> m_layers = nullptr;
        // Physical device (GPU)
        VkPhysicalDevice m_physicalDevice = {};
        // Logical device
        VkDevice m_logicalDevice = {};

        // Initialise vulkan
        void InitVulkan();
        // Create vulkan instance
        void CreateVKInstance();
        // Pick a GPU
        void PickPhysicalDevice();
        // Get queue families
        static Vk::QueueFamilyIndex FindQueueFamilies(VkPhysicalDevice device);
        // Create a logical device
        void CreateLogicalDevice();
    };
}

#endif
