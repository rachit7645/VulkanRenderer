#ifndef APP_INSTANCE_H
#define APP_INSTANCE_H

#include <vulkan/vulkan.h>

#include "Window.h"

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
        VkInstance vkInstance = {};

        // Initialise vulkan
        void InitVulkan();
        // Create vulkan instance
        void CreateVKInstance();
    };
}

#endif
