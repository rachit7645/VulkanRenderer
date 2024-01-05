/*
 *    Copyright 2023 - 2024 Rachit Khandelwal
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
        u32 extensionCount = 0;
        SDL_Vulkan_GetInstanceExtensions
        (
            window,
            &extensionCount,
            nullptr
        );

        auto instanceExtensions = reinterpret_cast<const char**>(SDL_malloc(sizeof(const char*) * extensionCount));
        auto extensionsLoaded = SDL_Vulkan_GetInstanceExtensions
        (
            window,
            &extensionCount,
            instanceExtensions
        );

        if (extensionsLoaded == SDL_FALSE)
        {
            Logger::VulkanError("Failed to load extensions!: {}\n", SDL_GetError());
        }

        auto extensionStrings = std::vector<const char*>(instanceExtensions, instanceExtensions + extensionCount);
        SDL_free(reinterpret_cast<void*>(instanceExtensions));

        #ifdef ENGINE_DEBUG
        extensionStrings.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        #endif

        return extensionStrings;
    }

    void Extensions::LoadInstanceFunctions(UNUSED VkInstance instance)
    {
        #ifdef ENGINE_DEBUG
        g_ExtensionState.p_CreateDebugUtilsMessengerEXT = LoadExtension<CreateDebugUtilsMessengerEXT>(
            instance, "vkCreateDebugUtilsMessengerEXT"
        );

        g_ExtensionState.p_DestroyDebugUtilsMessengerEXT = LoadExtension<DestroyDebugUtilsMessengerEXT>(
            instance, "vkDestroyDebugUtilsMessengerEXT"
        );
        #endif
    }

    bool Extensions::CheckDeviceExtensionSupport(VkPhysicalDevice device, const std::span<const char* const> requiredExtensions)
    {
        u32 extensionCount = 0;
        vkEnumerateDeviceExtensionProperties
        (
            device,
            nullptr,
            &extensionCount,
            nullptr
        );

        auto availableExtensions = std::vector<VkExtensionProperties>(extensionCount);
        vkEnumerateDeviceExtensionProperties
        (
            device,
            nullptr,
            &extensionCount,
            availableExtensions.data()
        );

        auto _requiredExtensions = std::set<std::string_view>(requiredExtensions.begin(), requiredExtensions.end());

        for (auto&& extension : availableExtensions)
        {
            _requiredExtensions.erase(extension.extensionName);
        }

        return _requiredExtensions.empty();
    }

    void Extensions::LoadDeviceFunctions(UNUSED VkDevice device)
    {
    }

    void Extensions::Destroy()
    {
        g_ExtensionState = {};
    }
}

extern "C"
{
    VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT
    (
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDebugUtilsMessengerEXT* pMessenger
    )
    {
        Vk::CreateDebugUtilsMessengerEXT fn = Vk::g_ExtensionState.p_CreateDebugUtilsMessengerEXT;
        return fn != nullptr ? fn(instance, pCreateInfo, pAllocator, pMessenger) : VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    
    VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT
    (
        VkInstance instance,
        VkDebugUtilsMessengerEXT messenger,
        const VkAllocationCallbacks* pAllocator
    )
    {
        Vk::DestroyDebugUtilsMessengerEXT fn = Vk::g_ExtensionState.p_DestroyDebugUtilsMessengerEXT;
        fn != nullptr ? fn(instance, messenger, pAllocator) : (void) fn;
    }
}