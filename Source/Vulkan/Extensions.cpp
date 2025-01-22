/*
 * Copyright (c) 2023 - 2025 Rachit
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Extensions.h"

#include <SDL3/SDL_vulkan.h>

#include "ExtensionState.h"
#include "ExtensionLoader.h"
#include "Util/Util.h"

namespace Vk
{
    // Internal extension function pointers (Global state, I know)
    static inline ExtensionState g_ExtensionState = {};

    std::vector<const char*> Extensions::LoadInstanceExtensions()
    {
        u32 extensionCount = 0;
        const auto instanceExtensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

        if (instanceExtensions == nullptr)
        {
            Logger::VulkanError("Failed to load extensions!: {}\n", SDL_GetError());
        }

        auto extensionStrings = std::vector<const char*>(instanceExtensions, instanceExtensions + extensionCount);

        #ifdef ENGINE_DEBUG
        extensionStrings.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        #endif

        return extensionStrings;
    }

    void Extensions::LoadInstanceFunctions(UNUSED VkInstance instance)
    {
        #ifdef ENGINE_ENABLE_VALIDATION
        g_ExtensionState.p_CreateDebugUtilsMessengerEXT = LoadExtension<PFN_vkCreateDebugUtilsMessengerEXT>(
            instance, "vkCreateDebugUtilsMessengerEXT"
        );

        g_ExtensionState.p_DestroyDebugUtilsMessengerEXT = LoadExtension<PFN_vkDestroyDebugUtilsMessengerEXT>(
            instance, "vkDestroyDebugUtilsMessengerEXT"
        );
        #endif

        #ifdef ENGINE_DEBUG
        g_ExtensionState.p_CmdBeginDebugUtilsLabelEXT = LoadExtension<PFN_vkCmdBeginDebugUtilsLabelEXT>(
            instance, "vkCmdBeginDebugUtilsLabelEXT"
        );

        g_ExtensionState.p_CmdEndDebugUtilsLabelEXT = LoadExtension<PFN_vkCmdEndDebugUtilsLabelEXT>(
            instance, "vkCmdEndDebugUtilsLabelEXT"
        );

        g_ExtensionState.p_QueueBeginDebugUtilsLabelEXT = LoadExtension<PFN_vkQueueBeginDebugUtilsLabelEXT>(
            instance, "vkQueueBeginDebugUtilsLabelEXT"
        );

        g_ExtensionState.p_QueueEndDebugUtilsLabelEXT = LoadExtension<PFN_vkQueueEndDebugUtilsLabelEXT>(
            instance, "vkQueueEndDebugUtilsLabelEXT"
        );

        g_ExtensionState.p_SetDebugUtilsObjectNameEXT = LoadExtension<PFN_vkSetDebugUtilsObjectNameEXT>(
            instance, "vkSetDebugUtilsObjectNameEXT"
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
    #ifdef ENGINE_DEBUG

    VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT
    (
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDebugUtilsMessengerEXT* pMessenger
    )
    {
        auto fn = Vk::g_ExtensionState.p_CreateDebugUtilsMessengerEXT;
        return fn != nullptr ? fn(instance, pCreateInfo, pAllocator, pMessenger) : VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    
    VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT
    (
        VkInstance instance,
        VkDebugUtilsMessengerEXT messenger,
        const VkAllocationCallbacks* pAllocator
    )
    {
        auto fn = Vk::g_ExtensionState.p_DestroyDebugUtilsMessengerEXT;
        fn != nullptr ? fn(instance, messenger, pAllocator) : (void) fn;
    }

    VKAPI_ATTR void VKAPI_CALL vkCmdBeginDebugUtilsLabelEXT(VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo)
    {
        auto fn = Vk::g_ExtensionState.p_CmdBeginDebugUtilsLabelEXT;
        fn != nullptr ? fn(commandBuffer, pLabelInfo) : (void) fn;
    }

    VKAPI_ATTR void VKAPI_CALL vkCmdEndDebugUtilsLabelEXT(VkCommandBuffer commandBuffer)
    {
        auto fn = Vk::g_ExtensionState.p_CmdEndDebugUtilsLabelEXT;
        fn != nullptr ? fn(commandBuffer) : (void) fn;
    }

    VKAPI_ATTR void VKAPI_CALL vkQueueBeginDebugUtilsLabelEXT(VkQueue queue, const VkDebugUtilsLabelEXT* pLabelInfo)
    {
        auto fn = Vk::g_ExtensionState.p_QueueBeginDebugUtilsLabelEXT;
        fn != nullptr ? fn(queue, pLabelInfo) : (void) fn;
    }

    VKAPI_ATTR void VKAPI_CALL vkQueueEndDebugUtilsLabelEXT(VkQueue queue)
    {
        auto fn = Vk::g_ExtensionState.p_QueueEndDebugUtilsLabelEXT;
        fn != nullptr ? fn(queue) : (void) fn;
    }

    VKAPI_ATTR VkResult VKAPI_CALL vkSetDebugUtilsObjectNameEXT(VkDevice device, const VkDebugUtilsObjectNameInfoEXT* pNameInfo)
    {
        auto fn = Vk::g_ExtensionState.p_SetDebugUtilsObjectNameEXT;
        return fn != nullptr ? fn(device, pNameInfo) : VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    #endif
}