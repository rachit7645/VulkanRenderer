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

#include "ValidationLayers.h"

#include "../Util/Log.h"

namespace Vk
{
#ifdef ENGINE_DEBUG
    ValidationLayers::ValidationLayers(const std::span<const char* const> layers)
    {
        // Check for availability
        if (!CheckLayers(layers))
        {
            // Log
            Logger::Error("{}\n", "Validation layers not found!");
        }

        // Creation data
        messengerInfo =
        {
            .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .pNext           = nullptr,
            .flags           = 0,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT   |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
            .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT    |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = ValidationLayers::DebugCallback,
            .pUserData       = nullptr,
        };
    }

    VkResult ValidationLayers::SetupMessenger(VkInstance instance)
    {
        // Create a messenger
        return vkCreateDebugUtilsMessengerEXT
        (
            instance,
            &messengerInfo,
            nullptr,
            &messenger
        );
    }

    bool ValidationLayers::CheckLayers(const std::span<const char* const> layers)
    {
        // Get layer count
        u32 layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        // Get layers
        auto availableLayers = std::vector<VkLayerProperties>(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        // Create layers string
        std::string layerDbg = fmt::format("Available vulkan validation layers: {}\n", layerCount);
        // Create unique layers set
        auto requiredLayers = std::set<std::string_view>(layers.begin(), layers.end());

        // Go over all layer properties
        for (auto&& layerProperties : availableLayers)
        {
            // Add to string
            layerDbg.append(fmt::format("- {}\n", layerProperties.layerName));
            // Remove from set
            requiredLayers.erase(layerProperties.layerName);
        }

        // Log
        Logger::Debug("{}", layerDbg);

        // Found all the layers!
        return requiredLayers.empty();
    }

    void ValidationLayers::DestroyMessenger(VkInstance instance)
    {
        // Destroy
        vkDestroyDebugUtilsMessengerEXT(instance, messenger, nullptr);
        messenger = nullptr;
    }

    // TODO: Improve validation layer debug callback
    VKAPI_ATTR VkBool32 VKAPI_CALL ValidationLayers::DebugCallback
    (
        UNUSED VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        UNUSED VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        UNUSED void* pUserData
    )
    {
        // Switch
        switch (severity)
        {
        // Ignore
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT:
            break;
        // Heed Vulkan's warnings
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            Logger::Vulkan("{}\n", pCallbackData->pMessage);
            break;
        // Exit at error
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            Logger::Vulkan("{}\n", pCallbackData->pMessage);
            std::exit(-1);
        }
        // Return
        return VK_TRUE;
    }
#endif
}