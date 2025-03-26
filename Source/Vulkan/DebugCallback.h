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

#ifndef DEBUG_CALLBACK_H
#define DEBUG_CALLBACK_H

#include <span>
#include <vulkan/vulkan.h>

namespace Vk
{
#ifdef ENGINE_DEBUG
    class DebugCallback
    {
    public:
        DebugCallback();

        void SetupMessenger(VkInstance instance);
        void Destroy(VkInstance instance) const;

        VkDebugUtilsMessengerEXT           messenger     = VK_NULL_HANDLE;
        VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {};
    private:
        static VKAPI_ATTR VkBool32 VKAPI_CALL Callback
        (
            VkDebugUtilsMessageSeverityFlagBitsEXT severity,
            VkDebugUtilsMessageTypeFlagsEXT type,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData
        );
    };
#endif
}

#endif