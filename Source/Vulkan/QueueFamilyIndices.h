#ifndef QUEUE_FAMILY_INDEX_H
#define QUEUE_FAMILY_INDEX_H

#include <optional>
#include <set>
#include <vulkan/vulkan.h>

#include "../Util/Util.h"

namespace Vk
{
    struct QueueFamilyIndices
    {
        // Default constructor
        QueueFamilyIndices() = default;
        // Find queue families for device
        QueueFamilyIndices(VkPhysicalDevice device, VkSurfaceKHR surface);
        // Graphics + Presentation family
        std::optional<u32> graphicsFamily = {};
        // Get all unique families
        [[nodiscard]] std::set<u32> GetUniqueFamilies() const;
        // Check if families are complete
        [[nodiscard]] bool IsComplete() const;
    };
}

#endif