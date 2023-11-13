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

#ifndef VK_CONSTANTS_H
#define VK_CONSTANTS_H

#include <array>
#include <numeric>
#include "Util/Util.h"

namespace Vk
{
    // Maximum frames in flight at a time
    constexpr usize FRAMES_IN_FLIGHT = 2;

    // Descriptor pool size
    constexpr std::array<VkDescriptorPoolSize, 2> DESCRIPTOR_POOL_SIZES =
    {
        VkDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         (1 << 4) * FRAMES_IN_FLIGHT),
        VkDescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (1 << 16) * FRAMES_IN_FLIGHT)
    };

    // Calculate descriptor pool size
    consteval auto GetDescriptorPoolSize() -> usize
    {
        // Accumulate
        return std::accumulate
        (
            DESCRIPTOR_POOL_SIZES.begin(),
            DESCRIPTOR_POOL_SIZES.end(),
            usize{0},
            [] (usize sum, const auto& poolSize) -> usize
            {
                // Add
                return sum + poolSize.descriptorCount;
            }
        );
    }
}

#endif