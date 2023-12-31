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

#ifndef VALIDATION_LAYERS_H
#define VALIDATION_LAYERS_H

#include <span>
#include <vulkan/vulkan.h>

namespace Vk
{
#ifdef ENGINE_DEBUG
    class ValidationLayers
    {
    public:
        // Default constructor
        ValidationLayers() = default;
        // Constructor
        explicit ValidationLayers(const std::span<const char* const> layers);

        // Setup messenger
        void SetupMessenger(VkInstance instance);
        // Destroy messenger
        void Destroy(VkInstance instance) const;

        // Debugging messenger
        VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
        // Messenger info
        VkDebugUtilsMessengerCreateInfoEXT messengerInfo = {};
    private:
        // Check if all layers are present
        [[nodiscard]] bool CheckLayers(const std::span<const char* const> layers);

        // Debug Callback
        static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback
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