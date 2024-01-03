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

#ifndef VK_UTIL_H
#define VK_UTIL_H

#include <functional>
#include <string_view>
#include <span>
#include <vulkan/vulkan.h>

#include "Util/Util.h"
#include "Context.h"
#include "CommandBuffer.h"

namespace Vk
{
    // Single use command buffer
    void ImmediateSubmit(const std::shared_ptr<Vk::Context>& context, const std::function<void(const Vk::CommandBuffer&)>& CmdFunction);

    // Find memory type
    u32 FindMemoryType
    (
        u32 typeFilter,
        VkMemoryPropertyFlags properties,
        const VkPhysicalDeviceMemoryProperties& memProperties
    );

    // Find supported format from list
    VkFormat FindSupportedFormat
    (
        VkPhysicalDevice physicalDevice,
        const std::span<const VkFormat> candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags features
    );

    // Check result (for ImGui)
    void CheckResult(VkResult result);
}

#endif
