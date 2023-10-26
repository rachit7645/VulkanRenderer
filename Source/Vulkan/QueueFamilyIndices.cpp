#include "QueueFamilyIndices.h"

namespace Vk
{
    QueueFamilyIndices::QueueFamilyIndices(VkPhysicalDevice device, VkSurfaceKHR surface)
    {
        // Get queue family count
        u32 queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties
        (
            device,
            &queueFamilyCount,
            nullptr
        );

        // Get queue families
        auto queueFamilies = std::vector<VkQueueFamilyProperties>(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties
        (
            device,
            &queueFamilyCount,
            queueFamilies.data()
        );

        // Loop over queue families
        for (usize i = 0; auto&& family : queueFamilies)
        {
            // Check for presentation support
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR
            (
                device,
                i,
                 surface,
                &presentSupport
            );

            // Check if family has graphics support
            if (presentSupport == VK_TRUE && family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                // Found graphics family!
                graphicsFamily = i;
            }

            // Check for completeness
            if (IsComplete())
            {
                // Found all the queue families we need
                break;
            }

            // Go to next family index
            ++i;
        }
    }

    std::set<u32> QueueFamilyIndices::GetUniqueFamilies() const
    {
        // Return all queue families
        return {graphicsFamily.value()};
    }

    bool QueueFamilyIndices::IsComplete() const
    {
        // Make sure both queues available
        return graphicsFamily.has_value();
    }
}