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

#include "Util.h"
#include "Util/Log.h"

namespace Vk
{
    void SingleTimeCmdBuffer(const std::shared_ptr<Vk::Context>& context, const std::function<void(VkCommandBuffer)>& CmdFunction)
    {
        // Allocate
        auto cmdBuffer = context->AllocateCommandBuffers(1, VK_COMMAND_BUFFER_LEVEL_PRIMARY)[0];

        // Begin info
        VkCommandBufferBeginInfo beginInfo =
        {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext            = nullptr,
            .flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr
        };
        // Begin
        vkBeginCommandBuffer(cmdBuffer, &beginInfo);

        // Execute user provided function
        std::invoke(CmdFunction, cmdBuffer);

        // End
        vkEndCommandBuffer(cmdBuffer);
        // Submit info
        VkSubmitInfo submitInfo =
        {
            .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext                = nullptr,
            .waitSemaphoreCount   = 0,
            .pWaitSemaphores      = nullptr,
            .pWaitDstStageMask    = nullptr,
            .commandBufferCount   = 1,
            .pCommandBuffers      = &cmdBuffer,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores    = nullptr
        };

        // Submit
        vkQueueSubmit(context->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(context->graphicsQueue);

        // Cleanup
        context->FreeCommandBuffers(std::span(&cmdBuffer, 1));
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
}