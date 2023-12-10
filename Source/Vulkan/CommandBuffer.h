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
        // Create command buffer
        CommandBuffer(const std::shared_ptr<Vk::Context>& context, VkCommandBufferLevel level);
        // Free command buffer
        void Free(const std::shared_ptr<Vk::Context>& context);

        // Default constructor
        CommandBuffer() = default;
        // Default destructor
        ~CommandBuffer() = default;

        // Deleted copy constructor
        CommandBuffer(const CommandBuffer&) = delete;
        // Deleted copy assignment operator
        CommandBuffer& operator=(const CommandBuffer&) = delete;

        // Move constructor
        CommandBuffer(CommandBuffer&& other) noexcept = default;
        // Move assignment operator
        CommandBuffer& operator=(CommandBuffer&& other) noexcept = default;

        // Begin recording command buffer
        void BeginRecording(VkCommandBufferUsageFlags usageFlags) const;
        // End recording
        void EndRecording() const;
        // Reset command buffer
        void Reset(VkCommandBufferResetFlags resetFlags) const;

        // Vulkan handle
        VkCommandBuffer handle = VK_NULL_HANDLE;
        // Level
        VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    };
}

#endif
