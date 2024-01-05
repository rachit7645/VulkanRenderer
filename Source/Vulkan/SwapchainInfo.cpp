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

#include "SwapchainInfo.h"

#include "Util.h"
#include "Util/Util.h"
#include "Util/Log.h"

namespace Vk
{
    SwapchainInfo::SwapchainInfo(VkPhysicalDevice device, VkSurfaceKHR surface)
    {
        Vk::CheckResult(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            device,
            surface,
            &capabilities),
            "Failed to assess surface capabilities!"
        );

        u32 formatCount = 0;
        Vk::CheckResult(vkGetPhysicalDeviceSurfaceFormatsKHR
        (
            device,
            surface,
            &formatCount,
            nullptr),
            "Failed to get surface format count!"
        );

        if (formatCount == 0)
        {
            Logger::VulkanError
            (
                "Failed to find any surface formats! [device={}] [surface={}]\n",
                std::bit_cast<void*>(device),
                std::bit_cast<void*>(surface)
            );
        }

        // Make sure to resize kids!
        formats.resize(formatCount);
        Vk::CheckResult(vkGetPhysicalDeviceSurfaceFormatsKHR
        (
            device,
            surface,
            &formatCount,
            formats.data()),
            "Failed to get surface formats!"
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
            Logger::VulkanError
            (
                "Failed to find any presentation modes! [device={}] [surface={}]\n",
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
    }
}