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

#include "SwapchainInfo.h"

#include <volk/volk.h>

#include "Util.h"
#include "Util/Types.h"
#include "Util/Log.h"

namespace Vk
{
    SwapchainInfo::SwapchainInfo(VkPhysicalDevice device, VkSurfaceKHR surface)
    {
        const VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo
        {
            .sType   = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
            .pNext   = nullptr,
            .surface = surface
        };

        capabilities = VkSurfaceCapabilities2KHR
        {
            .sType               = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR,
            .pNext               = nullptr,
            .surfaceCapabilities = {}
        };

        Vk::CheckResult(vkGetPhysicalDeviceSurfaceCapabilities2KHR(
            device,
            &surfaceInfo,
            &capabilities),
            "Failed to assess surface capabilities!"
        );

        u32 presentModeCount = 0;
        Vk::CheckResult(vkGetPhysicalDeviceSurfacePresentModesKHR
        (
            device,
            surface,
            &presentModeCount,
            nullptr),
            "Failed to get presentation mode count!"
        );

        if (presentModeCount == 0)
        {
            Logger::Error
            (
                "Failed to find any presentation modes! [Device={}] [Surface={}]\n",
                std::bit_cast<void*>(device),
                std::bit_cast<void*>(surface)
            );
        }

        // Make sure to resize
        presentModes.resize(presentModeCount);
        Vk::CheckResult(vkGetPhysicalDeviceSurfacePresentModesKHR
        (
            device,
            surface,
            &presentModeCount,
            presentModes.data()),
            "Failed to get presentation modes!"
        );

        u32 formatCount = 0;
        Vk::CheckResult(vkGetPhysicalDeviceSurfaceFormats2KHR
        (
            device,
            &surfaceInfo,
            &formatCount,
            nullptr),
            "Failed to get surface format count!"
        );

        if (formatCount == 0)
        {
            Logger::Error
            (
                "Failed to find any surface formats! [Device={}] [Surface={}]\n",
                std::bit_cast<void*>(device),
                std::bit_cast<void*>(surface)
            );
        }

        // Make sure to resize kids!
        formats.resize(formatCount);

        for (auto& format : formats)
        {
            format = VkSurfaceFormat2KHR
            {
                .sType         = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR,
                .pNext         = nullptr,
                .surfaceFormat = {}
            };
        }

        Vk::CheckResult(vkGetPhysicalDeviceSurfaceFormats2KHR
        (
            device,
            &surfaceInfo,
            &formatCount,
            formats.data()),
            "Failed to get surface formats!"
        );
    }
}