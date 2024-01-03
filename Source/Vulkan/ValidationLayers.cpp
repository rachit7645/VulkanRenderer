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

#include "ValidationLayers.h"

#include "../Util/Log.h"
#include "Util.h"

namespace Vk
{
#ifdef ENGINE_DEBUG
    ValidationLayers::ValidationLayers(const std::span<const char* const> layers)
    {
        if (!CheckLayers(layers))
        {
            Logger::Error("{}\n", "Validation layers not found!");
        }

        messengerInfo =
        {
            .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .pNext           = nullptr,
            .flags           = 0,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT     |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT  |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = ValidationLayers::DebugCallback,
            .pUserData       = nullptr,
        };
    }

    void ValidationLayers::SetupMessenger(VkInstance instance)
    {
        // Create a messenger
        Vk::CheckResult(vkCreateDebugUtilsMessengerEXT(
            instance,
            &messengerInfo,
            nullptr,
            &messenger),
            "Failed to set up debug messenger!"
        );
    }

    bool ValidationLayers::CheckLayers(const std::span<const char* const> layers)
    {
        u32 layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        // Get layers
        auto availableLayers = std::vector<VkLayerProperties>(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        // Create unique layers set
        auto requiredLayers = std::set<std::string_view>(layers.begin(), layers.end());

        // Go over all layer properties
        for (auto&& layerProperties : availableLayers)
        {
            // Remove from set
            requiredLayers.erase(layerProperties.layerName);
        }

        // Check if we found all the required layers
        return requiredLayers.empty();
    }

    void ValidationLayers::Destroy(VkInstance instance) const
    {
        // Destroy
        vkDestroyDebugUtilsMessengerEXT(instance, messenger, nullptr);
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
        // Heed Vulkan's warnings
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            Logger::Vulkan("{}\n", pCallbackData->pMessage);
            break;
        // Exit at error
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            Logger::VulkanError("{}\n", pCallbackData->pMessage);
            break;
        // Ignore
        default:
            break;
        }

        // Return
        return VK_TRUE;
    }
#endif
}