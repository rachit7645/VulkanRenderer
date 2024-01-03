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

#include "CommandBuffer.h"

namespace Vk
{
    CommandBuffer::CommandBuffer(const std::shared_ptr<Vk::Context>& context, VkCommandBufferLevel level)
        : level(level)
    {
        // Command buffer allocation data
        VkCommandBufferAllocateInfo allocInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext              = nullptr,
            .commandPool        = context->commandPool,
            .level              = level,
            .commandBufferCount = 1
        };
        // Allocate
        vkAllocateCommandBuffers(context->device, &allocInfo, &handle);
    }

    void CommandBuffer::Free(const std::shared_ptr<Vk::Context>& context)
    {
        // Free
        vkFreeCommandBuffers
        (
            context->device,
            context->commandPool,
            1,
            &handle
        );
    }

    void CommandBuffer::BeginRecording(VkCommandBufferUsageFlags usageFlags) const
    {
        // Begin info
        VkCommandBufferBeginInfo beginInfo =
        {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext            = nullptr,
            .flags            = usageFlags,
            .pInheritanceInfo = nullptr
        };
        // Begin
        vkBeginCommandBuffer(handle, &beginInfo);
    }

    void CommandBuffer::EndRecording() const
    {
        // End
        vkEndCommandBuffer(handle);
    }

    void CommandBuffer::Reset(VkCommandBufferResetFlags resetFlags) const
    {
        // Reset
        vkResetCommandBuffer(handle, resetFlags);
    }
}