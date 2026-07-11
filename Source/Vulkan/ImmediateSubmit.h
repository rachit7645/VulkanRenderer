/*
 * Copyright (c) 2023 - 2026 Rachit
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

#ifndef IMMEDIATE_SUBMIT_H
#define IMMEDIATE_SUBMIT_H

#include <source_location>
#include <vulkan/vulkan.h>

#include "CommandBuffer.h"
#include "CommandBufferAllocator.h"
#include "Util.h"
#include "DebugUtils.h"
#include "Util/SourceLocation.h"
#include "Externals/FMT.h"

namespace Vk
{
    template<typename F>
    void ImmediateSubmit
    (
        VkDevice device,
        VkQueue queue,
        Vk::CommandBufferAllocator& cmdBufferAllocator,
        F&& CmdFunction,
        const std::source_location location = std::source_location::current()
    )
    {
        const auto cmdBuffer = cmdBufferAllocator.AllocateGlobalCommandBuffer(device, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        constexpr VkFenceCreateInfo fenceCreateInfo =
        {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0
        };

        VkFence fence = VK_NULL_HANDLE;

        Vk::CheckResult(vkCreateFence(
            device,
            &fenceCreateInfo,
            nullptr,
            &fence),
            "Failed to create fence!"
        );

        const auto name = fmt::format("ImmediateSubmit/{}", Util::GetFunctionName(location));

        Vk::SetDebugName(device, fence, name);

        cmdBuffer.Reset(0);

        cmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
            Vk::BeginLabel(cmdBuffer, name, glm::vec4(glm::vec3(0.0f), 1.0f));
                std::forward<F>(CmdFunction)(cmdBuffer);
            Vk::EndLabel(cmdBuffer);
        cmdBuffer.EndRecording();

        const VkCommandBufferSubmitInfo cmdBufferInfo =
        {
            .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .pNext         = nullptr,
            .commandBuffer = cmdBuffer.handle,
            .deviceMask    = 0
        };

        const VkSubmitInfo2 submitInfo =
        {
            .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .pNext                    = nullptr,
            .flags                    = 0,
            .waitSemaphoreInfoCount   = 0,
            .pWaitSemaphoreInfos      = nullptr,
            .commandBufferInfoCount   = 1,
            .pCommandBufferInfos      = &cmdBufferInfo,
            .signalSemaphoreInfoCount = 0,
            .pSignalSemaphoreInfos    = nullptr
        };

        Vk::CheckResult(vkQueueSubmit2(
            queue,
            1,
            &submitInfo,
            fence),
            "Failed to submit immediate command buffer!"
        );

        Vk::CheckResult(vkWaitForFences(
            device,
            1,
            &fence,
            VK_TRUE,
            std::numeric_limits<u64>::max()),
            "Error while waiting for command buffer to be executed!"
        );

        vkDestroyFence(device, fence, nullptr);
        cmdBufferAllocator.FreeGlobalCommandBuffer(cmdBuffer);
    }
}

#endif
