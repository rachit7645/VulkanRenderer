#include "QueueFamilyIndices.h"

namespace Vk
{
    bool QueueFamilyIndices::IsComplete()
    {
        // Make sure both queues available
        return graphicsFamily.has_value() && presentFamily.has_value();
    }

    bool QueueFamilyIndices::IsPerformant()
    {
        // If our chosen queue supports graphics AND presentation, its more performant
        return graphicsFamily == presentFamily;
    }
}