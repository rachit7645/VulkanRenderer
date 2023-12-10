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
    void SingleTimeCmdBuffer(const std::shared_ptr<Vk::Context>& context, const std::function<void(const Vk::CommandBuffer&)>& CmdFunction)
    {
        // Create command buffer
        auto cmdBuffer = Vk::CommandBuffer(context, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        cmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        // Execute user provided function
        std::invoke(CmdFunction, cmdBuffer);
        // End
        cmdBuffer.EndRecording();

        // Submit info
        VkSubmitInfo submitInfo =
        {
            .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext                = nullptr,
            .waitSemaphoreCount   = 0,
            .pWaitSemaphores      = nullptr,
            .pWaitDstStageMask    = nullptr,
            .commandBufferCount   = 1,
            .pCommandBuffers      = &cmdBuffer.handle,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores    = nullptr
        };

        // Submit
        vkQueueSubmit(context->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
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
        Logger::Error("Failed to find suitable memory type!");
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