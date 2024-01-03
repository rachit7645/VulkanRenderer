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

#include <vulkan/vk_enum_string_helper.h>

#include "Util.h"
#include "Util/Log.h"

namespace Vk
{
    void ImmediateSubmit(const std::shared_ptr<Vk::Context>& context, const std::function<void(const Vk::CommandBuffer&)>& CmdFunction)
    {
        // Create command buffer
        auto cmdBuffer = Vk::CommandBuffer(context, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        cmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        // Execute user provided function
        CmdFunction(cmdBuffer);
        // End
        cmdBuffer.EndRecording();

        // Command buffer info
        VkCommandBufferSubmitInfo cmdBufferInfo =
        {
            .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .pNext         = nullptr,
            .commandBuffer = cmdBuffer.handle,
            .deviceMask    = 0
        };

        // Submit info
        VkSubmitInfo2 submitInfo =
        {
            .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .pNext                    = nullptr,
            .flags                    = 0,
            .waitSemaphoreInfoCount   = 0,
            .pWaitSemaphoreInfos      = nullptr,
            .commandBufferInfoCount   = 1,
            .pCommandBufferInfos      = &cmdBufferInfo,
            .signalSemaphoreInfoCount = 0,
            .pSignalSemaphoreInfos    = nullptr
        };

        // Submit
        vkQueueSubmit2(context->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(context->graphicsQueue);

        // Cleanup
        cmdBuffer.Free(context);
    }

    u32 FindMemoryType
    (
        u32 typeFilter,
        VkMemoryPropertyFlags properties,
        const VkPhysicalDeviceMemoryProperties& memProperties
    )
    {
        // Search through memory types
        for (u32 i = 0; i < memProperties.memoryTypeCount; ++i)
        {
            // Type check
            bool typeCheck = typeFilter & (0x1 << i);
            // Property check
            bool propertyCheck = (memProperties.memoryTypes[i].propertyFlags & properties) == properties;
            // Check
            if (typeCheck && propertyCheck)
            {
                // Found memory!
                return i;
            }
        }

        // If we didn't find memory, terminate
        Logger::VulkanError("Failed to find suitable memory type! [properties={}]", string_VkMemoryPropertyFlags(properties));
    }

    VkFormat FindSupportedFormat
    (
        VkPhysicalDevice physicalDevice,
        const std::span<const VkFormat> candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags features
    )
    {
        // Loop over formats
        for (auto format : candidates)
        {
            // Get format properties
            VkFormatProperties properties = {};
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);

            // Linear tiling
            bool isValidLinear = tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & features) == features;
            // Optimal tiling
            bool isValidOptimal = tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & features) == features;

            // Return if valid
            if (isValidLinear || isValidOptimal)
            {
                // Found valid format
                return format;
            }
        }

        // Throw error if no format was suitable
        Logger::Error
        (
            "No valid formats found! [physicalDevice={}] [tiling={}] [features={}]\n",
            std::bit_cast<void*>(physicalDevice),
            string_VkImageTiling(tiling),
            string_VkFormatFeatureFlags(features)
        );
    }

    void CheckResult(VkResult result)
    {
        // Check
        if (result != VK_SUCCESS)
        {
            // Log error
            Logger::VulkanError
            (
                "Result is {}! (Expected: {})\n",
                string_VkResult(result),
                string_VkResult(VK_SUCCESS)
            );
        }
    }
}