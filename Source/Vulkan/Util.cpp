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

#include <vulkan/vk_enum_string_helper.h>

#include "Util.h"
#include "Util/Log.h"
#include "Util/SourceLocation.h"

namespace Vk
{
    void ImmediateSubmit
    (
        VkDevice device,
        VkQueue queue,
        VkCommandPool cmdPool,
        const std::function<void(const Vk::CommandBuffer&)>& CmdFunction,
        UNUSED const std::source_location location
    )
    {
        auto cmdBuffer = Vk::CommandBuffer
        (
            device,
            cmdPool,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY
        );

        VkFenceCreateInfo fenceCreateInfo =
        {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0
        };

        VkFence fence;
        vkCreateFence(device, &fenceCreateInfo, nullptr, &fence);

        #ifdef ENGINE_DEBUG
        auto name = fmt::format("ImmediateSubmit/{}", Util::GetFunctionName(location));

        VkDebugUtilsObjectNameInfoEXT nameInfo =
        {
            .sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .pNext        = nullptr,
            .objectType   = VK_OBJECT_TYPE_UNKNOWN,
            .objectHandle = 0,
            .pObjectName  = name.c_str()
        };

        nameInfo.objectType   = VK_OBJECT_TYPE_COMMAND_BUFFER;
        nameInfo.objectHandle = std::bit_cast<u64>(cmdBuffer.handle);
        vkSetDebugUtilsObjectNameEXT(device, &nameInfo);

        nameInfo.objectType   = VK_OBJECT_TYPE_FENCE;
        nameInfo.objectHandle = std::bit_cast<u64>(fence);
        vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
        #endif

        cmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        #ifdef ENGINE_DEBUG
        const VkDebugUtilsLabelEXT label =
        {
            .sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
            .pNext      = nullptr,
            .pLabelName = name.c_str(),
            .color      = {}
        };

        vkCmdBeginDebugUtilsLabelEXT(cmdBuffer.handle, &label);
        #endif

        CmdFunction(cmdBuffer);

        #ifdef ENGINE_DEBUG
        vkCmdEndDebugUtilsLabelEXT(cmdBuffer.handle);
        #endif

        cmdBuffer.EndRecording();

        VkCommandBufferSubmitInfo cmdBufferInfo =
        {
            .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .pNext         = nullptr,
            .commandBuffer = cmdBuffer.handle,
            .deviceMask    = 0
        };

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

        Vk::CheckResult(vkQueueSubmit2(
            queue,
            1,
            &submitInfo,
            fence),
            "Failed to submit immediate command buffer!"
        );

        Vk::CheckResult(vkWaitForFences(
            device,
            1,
            &fence,
            VK_TRUE,
            std::numeric_limits<u64>::max()),
            "Error while waiting for command buffer to be executed!"
        );

        vkDestroyFence(device, fence, nullptr);
        cmdBuffer.Free(device, cmdPool);
    }

    VkFormat FindSupportedFormat
    (
        VkPhysicalDevice physicalDevice,
        const std::span<const VkFormat> candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags2 features
    )
    {
        for (auto format : candidates)
        {
            VkFormatProperties3 properties3 = {};
            properties3.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3;
            properties3.pNext = nullptr;

            VkFormatProperties2 properties2 = {};
            properties2.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
            properties2.pNext = &properties3;

            vkGetPhysicalDeviceFormatProperties2(physicalDevice, format, &properties2);

            bool isValidLinear  = (tiling == VK_IMAGE_TILING_LINEAR)  && ((properties3.linearTilingFeatures  & features) == features);
            bool isValidOptimal = (tiling == VK_IMAGE_TILING_OPTIMAL) && ((properties3.optimalTilingFeatures & features) == features);

            if (isValidLinear || isValidOptimal)
            {
                return format;
            }
        }

        // No format was suitable
        Logger::VulkanError
        (
            "No valid formats found! [physicalDevice={}] [tiling={}] [features={}]\n",
            std::bit_cast<void*>(physicalDevice),
            string_VkImageTiling(tiling),
            string_VkFormatFeatureFlags(features)
        );
    }

    void CheckResult(VkResult result, const std::string_view message)
    {
        if (result != VK_SUCCESS)
        {
            Logger::VulkanError("[{}] {}\n", string_VkResult(result), message.data());
        }
    }

    void CheckResult(VkResult result)
    {
        CheckResult(result, "ImGui Error!");
    }
}