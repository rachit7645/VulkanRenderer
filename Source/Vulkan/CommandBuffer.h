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

#ifndef COMMAND_BUFFER_H
#define COMMAND_BUFFER_H

#include <memory>
#include <vulkan/vulkan.h>

#include "Context.h"

namespace Vk
{
    class CommandBuffer
    {
    public:
        CommandBuffer() = default;
        CommandBuffer(VkDevice device, VkCommandPool cmdPool, VkCommandBufferLevel level);
        CommandBuffer(VkCommandBuffer handle, VkCommandBufferLevel level);

        void Free(VkDevice device, VkCommandPool cmdPool);

        void BeginRecording(VkCommandBufferUsageFlags usageFlags) const;
        void EndRecording() const;

        void Reset(VkCommandBufferResetFlags resetFlags) const;

        static std::vector<CommandBuffer> Allocate
        (
            u32 count,
            VkDevice device,
            VkCommandPool cmdPool,
            VkCommandBufferLevel level
        );

        static void Free(VkDevice device, VkCommandPool cmdPool, const std::span<const CommandBuffer> cmdBuffers);

        VkCommandBuffer      handle = VK_NULL_HANDLE;
        VkCommandBufferLevel level  = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    };
}

#endif
