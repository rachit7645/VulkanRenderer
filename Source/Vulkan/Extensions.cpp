#include "Extensions.h"

#include <SDL_vulkan.h>

namespace Vk
{
    // Extension loader
    inline std::unique_ptr<Extensions> extensions = nullptr;

    // Template to load function
    template <typename T>
    T LoadExtension(VkInstance& instance, std::string_view name)
    {
        // Load and return
        return reinterpret_cast<T>(vkGetInstanceProcAddr(instance, name.data()));
    }

    void Extensions::Load(VkInstance& instance)
    {
        // Allocate extensions
        extensions = std::make_unique<Extensions>();
        // Load debug message functions
        extensions->p_vkCreateDebugUtilsMessengerEXT = LoadExtension<PFN_vkCreateDebugUtilsMessengerEXT >(instance, "vkCreateDebugUtilsMessengerEXT");
        extensions->p_vkDestroyDebugUtilsMessengerEX = LoadExtension<PFN_vkDestroyDebugUtilsMessengerEXT>(instance, "vkDestroyDebugUtilsMessengerEXT");
    }

    void Extensions::Destroy()
    {
        // Reset
        extensions.reset();
    }
}

VkResult vkCreateDebugUtilsMessengerEXT
(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pMessenger
)
{
    // Get function
    PFN_vkCreateDebugUtilsMessengerEXT fn = Vk::extensions->p_vkCreateDebugUtilsMessengerEXT;
    // Call
    return fn != nullptr ? fn(instance, pCreateInfo, pAllocator, pMessenger) : VK_ERROR_EXTENSION_NOT_PRESENT;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT
(
    VkInstance instance,
    VkDebugUtilsMessengerEXT messenger,
    const VkAllocationCallbacks* pAllocator
)
{
    // Get function
    PFN_vkDestroyDebugUtilsMessengerEXT fn = Vk::extensions->p_vkDestroyDebugUtilsMessengerEX;
    // Call
    if (fn != nullptr) fn(instance, messenger, pAllocator);
}