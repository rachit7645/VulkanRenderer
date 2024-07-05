/*
 *    Copyright 2023 - 2024 Rachit Khandelwal
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

#include "CommandBuffer.h"

#include "Util.h"

namespace Vk
{
    CommandBuffer::CommandBuffer(const Vk::Context& context, VkCommandBufferLevel level, const std::string_view name)
        : level(level),
          m_name(name)
    {
        const VkCommandBufferAllocateInfo allocInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext              = nullptr,
            .commandPool        = context.commandPool,
            .level              = level,
            .commandBufferCount = 1
        };

        Vk::CheckResult(vkAllocateCommandBuffers(
            context.device,
            &allocInfo,
            &handle),
            "Failed to allocate command buffers!"
        );
    }

    void CommandBuffer::Free(const Vk::Context& context)
    {
        vkFreeCommandBuffers
        (
            context.device,
            context.commandPool,
            1,
            &handle
        );
    }

    void CommandBuffer::BeginRecording(VkCommandBufferUsageFlags usageFlags) const
    {
        const VkCommandBufferBeginInfo beginInfo =
        {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext            = nullptr,
            .flags            = usageFlags,
            .pInheritanceInfo = nullptr
        };

        Vk::CheckResult(vkBeginCommandBuffer(handle, &beginInfo), "Failed to begin recording command buffer!");

        #ifdef ENGINE_DEBUG
        const VkDebugUtilsLabelEXT label =
        {
            .sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
            .pNext      = nullptr,
            .pLabelName = m_name.c_str(),
            .color      = {}
        };

        vkCmdBeginDebugUtilsLabelEXT(handle, &label);
        #endif
    }

    void CommandBuffer::EndRecording() const
    {
        #ifdef ENGINE_DEBUG
        vkCmdEndDebugUtilsLabelEXT(handle);
        #endif
        Vk::CheckResult(vkEndCommandBuffer(handle), "Failed to end command buffer recording!");
    }

    void CommandBuffer::Reset(VkCommandBufferResetFlags resetFlags) const
    {
        Vk::CheckResult(vkResetCommandBuffer(handle, resetFlags), "Failed to reset command buffer!");
    }
}