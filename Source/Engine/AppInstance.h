#ifndef APP_INSTANCE_H
#define APP_INSTANCE_H

#include <vector>
#include <vulkan/vulkan.h>

#include "Window.h"
#include "../Util/Util.h"

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
        // Debugging messenger
        VkDebugUtilsMessengerEXT m_messenger = {};

        // Initialise vulkan
        void InitVulkan();

        // Create vulkan instance
        void CreateVKInstance();
        // Get extensions
        std::vector<const char*> GetExtensions();
        // Get validation layers
        static void LoadValidationLayers(const std::vector<const char*>& layers);
        // Check validation layer support
        static bool AreValidationLayersSupported(const std::vector<const char*>& layers);

        // Setup debug messages
        void SetupDebugCallback();

        // Debug Callback
        static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback
        (
            VkDebugUtilsMessageSeverityFlagBitsEXT severity,
            VkDebugUtilsMessageTypeFlagsEXT type,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData
        );
    };
}

#endif
