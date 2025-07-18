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

#include <volk/volk.h>

#include "Util/Types.h"
#include "Util/Log.h"
#include "Externals/UnorderedDense.h"
#include "Externals/SDL.h"

namespace Vk
{
    std::vector<const char*> LoadInstanceExtensions(const std::span<const char* const> requiredExtensions)
    {
        u32 extensionCount = 0;
        const auto instanceExtensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

        if (instanceExtensions == nullptr)
        {
            Logger::Error("Failed to load extensions!: {}\n", SDL_GetError());
        }

        auto extensionStrings = std::vector(instanceExtensions, instanceExtensions + extensionCount);

        extensionStrings.insert(extensionStrings.end(), requiredExtensions.begin(), requiredExtensions.end());

        return extensionStrings;
    }

    bool CheckDeviceExtensionSupport(VkPhysicalDevice device, const std::span<const char* const> requiredExtensions)
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

        auto _requiredExtensions = ankerl::unordered_dense::set<std::string_view>(requiredExtensions.begin(), requiredExtensions.end());

        for (const auto& extension : availableExtensions)
        {
            _requiredExtensions.erase(extension.extensionName);
        }

        return _requiredExtensions.empty();
    }
}