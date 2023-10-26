#include "SwapChainInfo.h"

#include "../Util/Util.h"

namespace Vk
{
    SwapChainInfo::SwapChainInfo(VkPhysicalDevice device, VkSurfaceKHR surface)
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