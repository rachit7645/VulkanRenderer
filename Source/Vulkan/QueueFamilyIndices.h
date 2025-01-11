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

#ifndef QUEUE_FAMILY_INDEX_H
#define QUEUE_FAMILY_INDEX_H

#include <optional>
#include <set>
#include <vulkan/vulkan.h>

#include "Util/Util.h"

namespace Vk
{
    struct QueueFamilyIndices
    {
        QueueFamilyIndices() = default;
        QueueFamilyIndices(VkPhysicalDevice device, VkSurfaceKHR surface);

        // Graphics + Presentation family
        std::optional<u32> graphicsFamily;

        [[nodiscard]] std::set<u32> GetUniqueFamilies() const;
        [[nodiscard]] bool IsComplete() const;
    };
}

#endif