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

#ifndef VK_UTIL_H
#define VK_UTIL_H

#include <functional>
#include <string_view>
#include <span>
#include <vulkan/vulkan.h>

#include "Util/Util.h"
#include "CommandBuffer.h"

// STD140 / STD430 alignment
#define VULKAN_GLSL_DATA alignas(16)

namespace Vk
{
    void ImmediateSubmit
    (
        VkDevice device,
        VkQueue queue,
        VkCommandPool cmdPool,
        const std::function<void(const Vk::CommandBuffer&)>& CmdFunction,
        const std::source_location location = std::source_location::current()
    );

    VkFormat FindSupportedFormat
    (
        VkPhysicalDevice physicalDevice,
        const std::span<const VkFormat> candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags2 features
    );

    void CheckResult(VkResult result, const std::string_view message);
    // For ImGui
    void CheckResult(VkResult result);
}

#endif
