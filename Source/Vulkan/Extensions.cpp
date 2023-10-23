#include "Extensions.h"

#include <SDL_vulkan.h>

#include "../Util/Util.h"
#include "../Util/Log.h"

namespace Vk
{
    // Template to load function
    template <typename T>
    T LoadExtension(VkInstance& instance, std::string_view name)
    {
        // Load and return
        auto extension = reinterpret_cast<T>(vkGetInstanceProcAddr(instance, name.data()));
        // Make sure we were able to load extension
        if (extension == nullptr)
        {
            // Log
            LOG_ERROR("Failed to load function \"{}\" for instance {} \n", name, reinterpret_cast<const void*>(&instance));
        }
        // Log
        LOG_DEBUG("Loaded function {} [address={}]\n", name, reinterpret_cast<void*>(extension));
        // Return
        return extension;
    }

    Extensions& Extensions::GetInstance()
    {
        // Create singleton and return
        static Extensions extensions;
        return extensions;
    }

    std::vector<const char*> Extensions::LoadExtensions(SDL_Window* window)
    {
        // Extensions count (from SDL)
        u32 extensionCount = 0;
        // Get extension count
        SDL_Vulkan_GetInstanceExtensions
        (
            window,
            &extensionCount,
            nullptr
        );

        // Allocate memory for extensions
        auto instanceExtensions = reinterpret_cast<const char**>(SDL_malloc(sizeof(const char*) * extensionCount));
        // Get extensions for real this time
        auto extensionsLoaded = SDL_Vulkan_GetInstanceExtensions
        (
            window,
            &extensionCount,
            instanceExtensions
        );

        // Make sure extensions were loaded
        if (extensionsLoaded == SDL_FALSE)
        {
            // Extension load failed, exit
            LOG_ERROR("Failed to load extensions!: {}\n", SDL_GetError());
        }

        // Convert to vector
        auto extensionStrings = std::vector<const char*>(instanceExtensions, instanceExtensions + extensionCount);
        // Free extensions
        SDL_free(instanceExtensions);

        // Add other extensions
        extensionStrings.emplace_back("VK_EXT_debug_utils");

        // Return
        return extensionStrings;
    }

    void Extensions::LoadFunctions(VkInstance& instance)
    {
        // Get function loader
        p_vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) SDL_Vulkan_GetVkGetInstanceProcAddr();
        // Check for errors
        if (p_vkGetInstanceProcAddr == nullptr)
        {
            // LOG
            LOG_ERROR("Failed to load function {}: \n", "vkGetInstanceProcAddr", SDL_GetError());
        }
        // Log
        LOG_DEBUG("Loaded function {} [address={}]\n", "vkGetInstanceProcAddr", reinterpret_cast<void*>(p_vkGetInstanceProcAddr));

        // Load debug message functions
        p_vkCreateDebugUtilsMessengerEXT  = LoadExtension<PFN_vkCreateDebugUtilsMessengerEXT >(instance, "vkCreateDebugUtilsMessengerEXT");
        p_vkDestroyDebugUtilsMessengerEXT = LoadExtension<PFN_vkDestroyDebugUtilsMessengerEXT>(instance, "vkDestroyDebugUtilsMessengerEXT");
    }

    void Extensions::Destroy()
    {
        // Reset
        GetInstance() = {};
    }
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char* pName)
{
    // Get function
    PFN_vkGetInstanceProcAddr fn = Vk::Extensions::GetInstance().p_vkGetInstanceProcAddr;
    // Call
    return fn(instance, pName);
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
    PFN_vkCreateDebugUtilsMessengerEXT fn = Vk::Extensions::GetInstance().p_vkCreateDebugUtilsMessengerEXT;
    // Call
    return fn(instance, pCreateInfo, pAllocator, pMessenger);
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT
(
    VkInstance instance,
    VkDebugUtilsMessengerEXT messenger,
    const VkAllocationCallbacks* pAllocator
)
{
    // Get function
    PFN_vkDestroyDebugUtilsMessengerEXT fn = Vk::Extensions::GetInstance().p_vkDestroyDebugUtilsMessengerEXT;
    // Call
    fn(instance, messenger, pAllocator);
}