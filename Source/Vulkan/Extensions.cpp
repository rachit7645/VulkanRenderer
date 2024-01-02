/*
 *    Copyright 2023 Rachit Khandelwal
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "Extensions.h"

#include <SDL_vulkan.h>

#include "ExtensionState.h"
#include "ExtensionLoader.h"
#include "Util/Util.h"

namespace Vk
{
    // Usings
    using CreateDebugUtilsMessengerEXT  = ExtensionState::CreateDebugUtilsMessengerEXT;
    using DestroyDebugUtilsMessengerEXT = ExtensionState::DestroyDebugUtilsMessengerEXT;

    // Internal extension function pointers (Global state, I know)
    static inline ExtensionState g_ExtensionState = {};

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
            Logger::Error("Failed to load extensions!: {}\n", SDL_GetError());
        }

        // Convert to vector
        auto extensionStrings = std::vector<const char*>(instanceExtensions, instanceExtensions + extensionCount);
        // Free extensions
        SDL_free(reinterpret_cast<void*>(instanceExtensions));

        // Add other extensions
        #ifdef ENGINE_DEBUG
        extensionStrings.emplace_back("VK_EXT_debug_utils");
        #endif

        // Return
        return extensionStrings;
    }

    void Extensions::LoadInstanceFunctions(VkInstance instance)
    {
        #ifdef ENGINE_DEBUG
        // Load debug utils creation function
        g_ExtensionState.p_CreateDebugUtilsMessengerEXT = LoadExtension<CreateDebugUtilsMessengerEXT>(
            instance, "vkCreateDebugUtilsMessengerEXT"
        );

        // Load debug utils destruction function
        g_ExtensionState.p_DestroyDebugUtilsMessengerEXT = LoadExtension<DestroyDebugUtilsMessengerEXT>(
            instance, "vkDestroyDebugUtilsMessengerEXT"
        );
        #endif
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

    void Extensions::Destroy()
    {
        // Reset
        g_ExtensionState = {};
    }
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
    Vk::CreateDebugUtilsMessengerEXT fn = Vk::g_ExtensionState.p_CreateDebugUtilsMessengerEXT;
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
    Vk::DestroyDebugUtilsMessengerEXT fn = Vk::g_ExtensionState.p_DestroyDebugUtilsMessengerEXT;
    // Call
    fn != nullptr ? fn(instance, messenger, pAllocator) : (void) fn;
}