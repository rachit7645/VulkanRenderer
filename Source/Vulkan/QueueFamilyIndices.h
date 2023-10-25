#ifndef QUEUE_FAMILY_INDEX_H
#define QUEUE_FAMILY_INDEX_H

#include <optional>

#include "../Util/Util.h"

namespace Vk
{
    struct QueueFamilyIndices
    {
        // Graphics family
        std::optional<u32> graphicsFamily;
        // Present family
        std::optional<u32> presentFamily;
        // Check if families are complete
        bool IsComplete();
        // Check if families are performant
        bool IsPerformant();
    };
}

#endif