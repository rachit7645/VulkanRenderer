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

#include "CommandBuffer.h"

#include <volk/volk.h>

#include "Util.h"

namespace Vk
{
    CommandBuffer::CommandBuffer(VkDevice device, VkCommandPool cmdPool, VkCommandBufferLevel level)
        : level(level)
    {
        const VkCommandBufferAllocateInfo allocInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext              = nullptr,
            .commandPool        = cmdPool,
            .level              = level,
            .commandBufferCount = 1
        };

        Vk::CheckResult(vkAllocateCommandBuffers(
            device,
            &allocInfo,
            &handle),
            "Failed to allocate command buffers!"
        );
    }

    CommandBuffer::CommandBuffer(VkCommandBuffer handle, VkCommandBufferLevel level)
        : handle(handle),
          level(level)
    {
    }

    void CommandBuffer::Free(VkDevice device, VkCommandPool cmdPool)
    {
        vkFreeCommandBuffers
        (
            device,
            cmdPool,
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
    }

    void CommandBuffer::EndRecording() const
    {
        Vk::CheckResult(vkEndCommandBuffer(handle), "Failed to end command buffer recording!");
    }

    void CommandBuffer::Reset(VkCommandBufferResetFlags resetFlags) const
    {
        Vk::CheckResult(vkResetCommandBuffer(handle, resetFlags), "Failed to reset command buffer!");
    }

    std::vector<CommandBuffer> CommandBuffer::Allocate
    (
        u32 count,
        VkDevice device,
        VkCommandPool cmdPool,
        VkCommandBufferLevel level
    )
    {
        std::vector<VkCommandBuffer> _cmdBuffers = {};
        _cmdBuffers.resize(count, VK_NULL_HANDLE);

        const VkCommandBufferAllocateInfo allocInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext              = nullptr,
            .commandPool        = cmdPool,
            .level              = level,
            .commandBufferCount = count
        };

        Vk::CheckResult(vkAllocateCommandBuffers(
            device,
            &allocInfo,
            _cmdBuffers.data()),
            "Failed to allocate command buffers!"
        );

        std::vector<Vk::CommandBuffer> cmdBuffers = {};
        cmdBuffers.reserve(count);

        std::ranges::transform
        (
            _cmdBuffers,
            std::back_inserter(cmdBuffers),
            [level] (VkCommandBuffer cmdBuffer)
            {
                return Vk::CommandBuffer(cmdBuffer, level);
            }
        );

        return cmdBuffers;
    }

    void CommandBuffer::Free(VkDevice device, VkCommandPool cmdPool, const std::span<const CommandBuffer> cmdBuffers)
    {
        std::vector<VkCommandBuffer> _cmdBuffers = {};
        _cmdBuffers.reserve(cmdBuffers.size());

        std::ranges::transform
        (
            cmdBuffers,
            std::back_inserter(_cmdBuffers),
            [] (const Vk::CommandBuffer& cmdBuffer)
            {
                return cmdBuffer.handle;
            }
        );

        vkFreeCommandBuffers
        (
            device,
            cmdPool,
            _cmdBuffers.size(),
            _cmdBuffers.data()
        );
    }
}
