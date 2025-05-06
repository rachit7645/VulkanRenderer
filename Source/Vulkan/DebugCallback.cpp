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

#include "DebugCallback.h"

#include "Util/Log.h"
#include "Util.h"

namespace Vk
{
#ifdef ENGINE_DEBUG
    DebugCallback::DebugCallback()
    {
        messengerInfo =
           {
            .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .pNext           = nullptr,
            .flags           = 0,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT    |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT     |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT  |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = DebugCallback::Callback,
            .pUserData       = nullptr,
        };
    }

    void DebugCallback::SetupMessenger(VkInstance instance)
    {
        Vk::CheckResult(vkCreateDebugUtilsMessengerEXT(
            instance,
            &messengerInfo,
            nullptr,
            &messenger),
            "Failed to set up debug messenger!"
        );
    }

    void DebugCallback::Destroy(VkInstance instance) const
    {
        vkDestroyDebugUtilsMessengerEXT(instance, messenger, nullptr);
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback::Callback
    (
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        ENGINE_UNUSED VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        ENGINE_UNUSED void* pUserData
    )
    {
        switch (severity)
        {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            Logger::VulkanError("{}\n", pCallbackData->pMessage);
            break;
        default:
            Logger::Vulkan("{}\n", pCallbackData->pMessage);
            break;
        }

        return VK_FALSE;
    }
#endif
}