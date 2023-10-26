#ifndef SWAP_CHAIN_INFO_H
#define SWAP_CHAIN_INFO_H

#include <vector>
#include <vulkan/vulkan.h>

namespace Vk
{
    struct SwapChainInfo
    {
        // Default constructor
        SwapChainInfo() = default;
        // Query swapchain info
        SwapChainInfo(VkPhysicalDevice device, VkSurfaceKHR surface);
        // Surface feature data
        VkSurfaceCapabilitiesKHR capabilities = {};
        // Available formats
        std::vector<VkSurfaceFormatKHR> formats = {};
        // Presentation modes
        std::vector<VkPresentModeKHR> presentModes = {};
    };
}

#endif
