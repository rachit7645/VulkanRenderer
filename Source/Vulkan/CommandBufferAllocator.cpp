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

#include "CommandBufferAllocator.h"

#include "DebugUtils.h"
#include "Util.h"

namespace Vk
{
    CommandBufferAllocator::CommandBufferAllocator(VkDevice device, u32 queueFamilyIndex)
        : m_queueFamilyIndex(queueFamilyIndex)
    {
        const VkCommandPoolCreateInfo resetCreateInfo =
        {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext            = nullptr,
            .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = m_queueFamilyIndex
        };

        Vk::CheckResult(vkCreateCommandPool(
            device,
            &resetCreateInfo,
            nullptr,
            &m_globalCommandPool),
            "Failed to create command pool!"
        );

        const VkCommandPoolCreateInfo createInfo =
        {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext            = nullptr,
            .flags            = 0,
            .queueFamilyIndex = m_queueFamilyIndex
        };

        for (auto& commandPool : m_commandPools)
        {
            Vk::CheckResult(vkCreateCommandPool(
                device,
                &createInfo,
                nullptr,
                &commandPool),
                "Failed to create command pool!"
            );
        }

        Vk::SetDebugName(device, m_globalCommandPool, fmt::format("QueueFamily{}/GlobalCommandPool", m_queueFamilyIndex));

        for (usize i = 0; i < m_commandPools.size(); ++i)
        {
            Vk::SetDebugName(device, m_commandPools[i], fmt::format("QueueFamily{}/CommandPool/FIF{}", m_queueFamilyIndex, i));
        }
    }

    Vk::CommandBuffer CommandBufferAllocator::AllocateGlobalCommandBuffer(VkDevice device, VkCommandBufferLevel level)
    {
        Vk::CommandBuffer cmdBuffer = {};

        if (!m_freedGlobalCommandBuffers.empty())
        {
            cmdBuffer = m_freedGlobalCommandBuffers.front();
            m_freedGlobalCommandBuffers.pop();
        }
        else
        {
            cmdBuffer = m_allocatedGlobalCommandBuffers.emplace_back(device, m_globalCommandPool, level);

            Vk::SetDebugName(device, cmdBuffer.handle, fmt::format("QueueFamily{}/GlobalCommandBuffer/{}", m_queueFamilyIndex, m_allocatedGlobalCommandBuffers.size() - 1));
        }

        return cmdBuffer;
    }

    void CommandBufferAllocator::FreeGlobalCommandBuffer(const Vk::CommandBuffer& commandBuffer)
    {
        m_freedGlobalCommandBuffers.push(commandBuffer);
    }

    Vk::CommandBuffer CommandBufferAllocator::AllocateCommandBuffer(usize FIF, VkDevice device, VkCommandBufferLevel level)
    {
        for (auto& [commandBuffer, isDirty] : m_allocatedCommandBuffers[FIF])
        {
            if (!isDirty)
            {
                isDirty = true;

                return commandBuffer;
            }
        }

        const auto cmdBuffer = m_allocatedCommandBuffers[FIF].emplace_back(Vk::CommandBuffer(device, m_commandPools[FIF], level), true).commandBuffer;

        Vk::SetDebugName(device, cmdBuffer.handle, fmt::format("QueueFamily{}/CommandBuffer/FIF{}/{}", m_queueFamilyIndex, FIF, m_allocatedCommandBuffers[FIF].size() - 1));

        return cmdBuffer;
    }

    void CommandBufferAllocator::ResetPool(usize FIF, VkDevice device)
    {
        Vk::CheckResult(vkResetCommandPool(device, m_commandPools[FIF], 0), "Failed to reset command pool!");

        for (auto& [_, isDirty] : m_allocatedCommandBuffers[FIF])
        {
            isDirty = false;
        }
    }

    void CommandBufferAllocator::Destroy(VkDevice device)
    {
        Vk::CheckResult(vkResetCommandPool(device, m_globalCommandPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT), "Failed to reset command pool!");

        vkDestroyCommandPool(device, m_globalCommandPool, nullptr);

        for (const auto commandPool : m_commandPools)
        {
            Vk::CheckResult(vkResetCommandPool(device, commandPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT), "Failed to reset command pool!");

            vkDestroyCommandPool(device, commandPool, nullptr);
        }
    }
}
