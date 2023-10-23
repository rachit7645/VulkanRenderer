#ifndef QUEUE_FAMILY_INDEX_H
#define QUEUE_FAMILY_INDEX_H

#include <optional>

#include "../Util/Util.h"

namespace Vk
{
    struct QueueFamilyIndex
    {
        // Graphics family
        std::optional<u32> graphicsFamily;
    };
}

#endif