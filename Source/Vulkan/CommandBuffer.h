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
        CommandBuffer(const Vk::Context& context, VkCommandBufferLevel level, const std::string_view name);
        void Free(const Vk::Context& context);

        CommandBuffer()  = default;
        ~CommandBuffer() = default;

        // No copying
        CommandBuffer(const CommandBuffer&) = delete;
        CommandBuffer& operator=(const CommandBuffer&) = delete;

        // Only moving
        CommandBuffer(CommandBuffer&& other) noexcept = default;
        CommandBuffer& operator=(CommandBuffer&& other) noexcept = default;

        void BeginRecording(VkCommandBufferUsageFlags usageFlags) const;
        void EndRecording() const;

        void Reset(VkCommandBufferResetFlags resetFlags) const;

        VkCommandBuffer      handle = VK_NULL_HANDLE;
        VkCommandBufferLevel level  = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    private:
        std::string m_name = {};
    };
}

#endif
