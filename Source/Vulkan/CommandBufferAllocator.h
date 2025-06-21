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

#ifndef COMMAND_BUFFER_ALLOCATOR_H
#define COMMAND_BUFFER_ALLOCATOR_H

#include <array>
#include <queue>
#include <vector>
#include <vulkan/vulkan.h>

#include "Constants.h"
#include "QueueFamilyIndices.h"
#include "CommandBuffer.h"

namespace Vk
{
    class CommandBufferAllocator
    {
    public:
        CommandBufferAllocator(VkDevice device, u32 queueFamilyIndex);

        [[nodiscard]] Vk::CommandBuffer AllocateGlobalCommandBuffer(VkDevice device, VkCommandBufferLevel level);
        void FreeGlobalCommandBuffer(const Vk::CommandBuffer& commandBuffer);

        [[nodiscard]] Vk::CommandBuffer AllocateCommandBuffer(usize FIF, VkDevice device, VkCommandBufferLevel level);

        void ResetPool(usize FIF, VkDevice device);

        void Destroy(VkDevice device);
    private:
        struct CommandBufferState
        {
            Vk::CommandBuffer commandBuffer = {};
            bool              isDirty       = false;
        };

        u32 m_queueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        VkCommandPool                                   m_globalCommandPool = VK_NULL_HANDLE;
        std::array<VkCommandPool, Vk::FRAMES_IN_FLIGHT> m_commandPools      = {};

        std::vector<Vk::CommandBuffer> m_allocatedGlobalCommandBuffers = {};
        std::queue<Vk::CommandBuffer>  m_freedGlobalCommandBuffers     = {};

        std::array<std::vector<CommandBufferState>, Vk::FRAMES_IN_FLIGHT> m_allocatedCommandBuffers = {};
    };
}

#endif
