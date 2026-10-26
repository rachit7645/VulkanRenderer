/*
 * Copyright (c) 2023 - 2026 Rachit
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

#ifndef VK_UTIL_H
#define VK_UTIL_H

#include <string_view>
#include <vulkan/vulkan.h>

#include "Engine/Scratch.h"
#include "Util/Types.h"

namespace Vk
{
    [[nodiscard]] usize CalculatePhysicalDeviceScore
    (
        VkInstance instance,
        VkPhysicalDevice physicalDevice,
        VkSurfaceKHR surface,
        const VkPhysicalDeviceProperties2& properties,
        const VkPhysicalDeviceFeatures2& features,
        Scratch::Allocator& scratchAllocator
    );

    [[nodiscard]] VkFormat FindSupportedFormat
    (
       VkPhysicalDevice physicalDevice,
       const std::span<const VkFormat> candidates,
       VkImageTiling tiling,
       VkFormatFeatureFlags2 features
    );

    [[nodiscard]] f64 GetTexelSize(VkFormat format);

    [[nodiscard]] VkDeviceSize GetImageSize(VkFormat format, u32 width, u32 height);

    void CheckResult(VkResult result, const std::string_view message);
}

#endif
