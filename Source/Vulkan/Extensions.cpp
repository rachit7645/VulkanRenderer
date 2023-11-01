#include "Extensions.h"

#include <SDL_vulkan.h>

#include "ExtensionState.h"
#include "ExtensionLoader.h"
#include "Util/Util.h"

namespace Vk
{
    // Internal extension function pointers (Global state, I know)
    inline ExtensionState g_ExtensionState = {};

    std::vector<const char*> Extensions::LoadInstanceExtensions(SDL_Window* window)
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

    void Extensions::LoadProcLoader()
    {
        // Get function loader
        g_ExtensionState.p_vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_Vulkan_GetVkGetInstanceProcAddr());
        // Check for errors
        if (g_ExtensionState.p_vkGetInstanceProcAddr == nullptr)
        {
            // LOG
            LOG_ERROR("Failed to load function {}: \n", "vkGetInstanceProcAddr", SDL_GetError());
        }
        // Log
        LOG_DEBUG
        (
            "Loaded function {} [address={}]\n",
            "vkGetInstanceProcAddr",
            reinterpret_cast<void*>(g_ExtensionState.p_vkGetInstanceProcAddr)
        );
    }

    void Extensions::LoadInstanceFunctions(VkInstance instance)
    {
        // Load instance loader
        LoadProcLoader();

        // Load debug utils creation function
        g_ExtensionState.p_vkCreateDebugUtilsMessengerEXT = LoadExtension<PFN_vkCreateDebugUtilsMessengerEXT>
        (
            instance,
            "vkCreateDebugUtilsMessengerEXT"
        );

        // Load debug utils destruction function
        g_ExtensionState.p_vkDestroyDebugUtilsMessengerEXT = LoadExtension<PFN_vkDestroyDebugUtilsMessengerEXT>
        (
            instance,
            "vkDestroyDebugUtilsMessengerEXT"
        );
    }

    bool Extensions::CheckDeviceExtensionSupport(VkPhysicalDevice device, const std::span<const char* const> requiredExtensions)
    {
        // Get device extension count
        u32 extensionCount = 0;
        vkEnumerateDeviceExtensionProperties
        (
            device,
            nullptr,
            &extensionCount,
            nullptr
        );

        // Get extensions
        auto availableExtensions = std::vector<VkExtensionProperties>(extensionCount);
        vkEnumerateDeviceExtensionProperties
        (
            device,
            nullptr,
            &extensionCount,
            availableExtensions.data()
        );

        // Set of unique extensions
        auto _requiredExtensions = std::set<std::string_view>(requiredExtensions.begin(), requiredExtensions.end());

        // Check each extension
        for (auto&& extension : availableExtensions)
        {
            // Erase
            _requiredExtensions.erase(extension.extensionName);
        }

        // Check if all required extensions were found
        return _requiredExtensions.empty();
    }

    void Extensions::LoadDeviceFunctions(UNUSED VkDevice device)
    {
    }

    Extensions::~Extensions()
    {
        // Reset
        g_ExtensionState = {};
    }
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char* pName)
{
    // Get function
    PFN_vkGetInstanceProcAddr fn = Vk::g_ExtensionState.p_vkGetInstanceProcAddr;
    // Call
    return fn(instance, pName);
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT
(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pMessenger
)
{
    // Get function
    PFN_vkCreateDebugUtilsMessengerEXT fn = Vk::g_ExtensionState.p_vkCreateDebugUtilsMessengerEXT;
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
    PFN_vkDestroyDebugUtilsMessengerEXT fn = Vk::g_ExtensionState.p_vkDestroyDebugUtilsMessengerEXT;
    // Call
    fn(instance, messenger, pAllocator);
}