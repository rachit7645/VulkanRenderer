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

    // Get number of sets to put in descriptor pool
    consteval usize GetDescriptorPoolSize()
    {
        // Descriptor sets list
        constexpr std::array<usize, 1> DESCRIPTOR_ALLOCATIONS =
        {
            FRAMES_IN_FLIGHT // Shared UBO
        };

        // Return sum
        return std::accumulate(DESCRIPTOR_ALLOCATIONS.begin(), DESCRIPTOR_ALLOCATIONS.end(), 0);
    }
}

#endif