#ifndef EXTENSION_STATE_H
#define EXTENSION_STATE_H

#include <vulkan/vulkan.h>

namespace Vk
{
    struct ExtensionState
    {
        // Function pointers
        PFN_vkGetInstanceProcAddr           p_vkGetInstanceProcAddr           = nullptr;
        PFN_vkCreateDebugUtilsMessengerEXT  p_vkCreateDebugUtilsMessengerEXT  = nullptr;
        PFN_vkDestroyDebugUtilsMessengerEXT p_vkDestroyDebugUtilsMessengerEXT = nullptr;
    };
}

#endif
