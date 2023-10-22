#ifndef EXTENSION_LOADER_H
#define EXTENSION_LOADER_H

#include <vulkan/vulkan.h>

namespace Vk
{
    class Extensions
    {
    public:
        // Load extensions
        static void Load(VkInstance& instance);
        // Destroy
        static void Destroy();

        // Function pointers
        PFN_vkCreateDebugUtilsMessengerEXT  p_vkCreateDebugUtilsMessengerEXT = nullptr;
        PFN_vkDestroyDebugUtilsMessengerEXT p_vkDestroyDebugUtilsMessengerEX = nullptr;

        // Constructor (DO NOT USE DIRECTLY)
        Extensions() = default;
    };
}

#endif
