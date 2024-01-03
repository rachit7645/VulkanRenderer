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

#include "SwapchainInfo.h"

#include "../Util/Util.h"

namespace Vk
{
    SwapchainInfo::SwapchainInfo(VkPhysicalDevice device, VkSurfaceKHR surface)
    {
        // Get capabilities
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &capabilities);

        // Get format count
        u32 formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR
        (
            device,
            surface,
            &formatCount,
            nullptr
        );

        // Get formats
        if (formatCount != 0)
        {
            // Make sure to resize!
            formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR
            (
                device,
                surface,
                &formatCount,
                formats.data()
            );
        }

        // Get presentation modes count
        u32 presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR
        (
            device,
            surface,
            &presentModeCount,
            nullptr
        );

        // Get presentation modes fr this time
        if (presentModeCount != 0)
        {
            // Make sure to resize
            presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR
            (
                device,
                surface,
                &presentModeCount,
                presentModes.data()
            );
        }
    }
}