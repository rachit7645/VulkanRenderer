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
#include <vulkan/vulkan.h>

#include "Util/Types.h"
#include "Externals/UnorderedDense.h"

namespace Vk
{
    class QueueFamilies
    {
    public:
        QueueFamilies() = default;
        QueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

        std::optional<u32> graphicsFamily = std::nullopt;
        std::optional<u32> computeFamily  = std::nullopt;

        [[nodiscard]] ankerl::unordered_dense::set<u32> GetUniqueFamilies() const;

        [[nodiscard]] bool HasRequiredFamilies() const;
        [[nodiscard]] bool HasAllFamilies() const;
    };
}

#endif