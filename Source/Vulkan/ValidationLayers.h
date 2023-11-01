#ifndef VALIDATION_LAYERS_H
#define VALIDATION_LAYERS_H

#include <span>
#include <vulkan/vulkan.h>

namespace Vk
{
#ifdef ENGINE_DEBUG
    class ValidationLayers
    {
    public:
        // Constructor
        explicit ValidationLayers(const std::span<const char* const> layers);

        // Setup messenger
        [[nodiscard]] VkResult SetupMessenger(VkInstance instance);
        // Destroy messenger
        void DestroyMessenger(VkInstance instance);

        // Messenger
        VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {};
        // Debugging messenger
        VkDebugUtilsMessengerEXT messenger = {};
    private:
        // Check if all layers are present
        [[nodiscard]] bool CheckLayers(const std::span<const char* const> layers);

        // Debug Callback
        static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback
        (
            VkDebugUtilsMessageSeverityFlagBitsEXT severity,
            VkDebugUtilsMessageTypeFlagsEXT type,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData
        );
    };
#endif
}

#endif