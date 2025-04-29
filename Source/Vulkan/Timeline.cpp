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

#include "Timeline.h"

#include "Util.h"
#include "DebugUtils.h"

namespace Vk
{
    Timeline::Timeline(VkDevice device)
    {
        const VkSemaphoreTypeCreateInfo semaphoreTypeCreateInfo =
        {
            .sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
            .pNext         = nullptr,
            .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
            .initialValue  = 0
        };

        const VkSemaphoreCreateInfo semaphoreInfo =
        {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = &semaphoreTypeCreateInfo,
            .flags = 0
        };

        Vk::CheckResult(vkCreateSemaphore(
            device,
            &semaphoreInfo,
            nullptr,
            &semaphore),
            "Failed to create timeline semaphore!"
        );

        Vk::SetDebugName(device, semaphore, "TimelineSemaphore");
    }

    void Timeline::AcquireImageToTimeline(usize frameIndex, VkQueue queue, VkSemaphore imageAcquire)
    {
        const VkSemaphoreSubmitInfo waitSemaphoreInfo =
        {
            .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext       = nullptr,
            .semaphore   = imageAcquire,
            .value       = 0,
            .stageMask   = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
            .deviceIndex = 0
        };

        const VkSemaphoreSubmitInfo signalSemaphoreInfo =
        {
            .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext       = nullptr,
            .semaphore   = semaphore,
            .value       = GetTimelineValue(frameIndex, TimelineStage::TIMELINE_STAGE_SWAPCHAIN_IMAGE_ACQUIRED),
            .stageMask   = VK_PIPELINE_STAGE_2_NONE,
            .deviceIndex = 0
        };

        const VkSubmitInfo2 submitInfo =
        {
            .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .pNext                    = nullptr,
            .flags                    = 0,
            .waitSemaphoreInfoCount   = 1,
            .pWaitSemaphoreInfos      = &waitSemaphoreInfo,
            .commandBufferInfoCount   = 0,
            .pCommandBufferInfos      = nullptr,
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos    = &signalSemaphoreInfo
        };

        Vk::CheckResult(vkQueueSubmit2(
            queue,
            1,
            &submitInfo,
            VK_NULL_HANDLE),
            "Failed to submit queue!"
        );
    }

    void Timeline::TimelineToRenderFinished(usize frameIndex, VkQueue queue, VkSemaphore renderFinished)
    {
        const VkSemaphoreSubmitInfo waitSemaphoreInfo =
        {
            .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext       = nullptr,
            .semaphore   = semaphore,
            .value       = GetTimelineValue(frameIndex, TimelineStage::TIMELINE_STAGE_RENDER_FINISHED),
            .stageMask   = VK_PIPELINE_STAGE_2_NONE,
            .deviceIndex = 0
        };

        const VkSemaphoreSubmitInfo signalSemaphoreInfo =
        {
            .sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext       = nullptr,
            .semaphore   = renderFinished,
            .value       = 0,
            .stageMask   = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
            .deviceIndex = 0
        };

        const VkSubmitInfo2 submitInfo =
        {
            .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .pNext                    = nullptr,
            .flags                    = 0,
            .waitSemaphoreInfoCount   = 1,
            .pWaitSemaphoreInfos      = &waitSemaphoreInfo,
            .commandBufferInfoCount   = 0,
            .pCommandBufferInfos      = nullptr,
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos    = &signalSemaphoreInfo
        };

        Vk::CheckResult(vkQueueSubmit2(
            queue,
            1,
            &submitInfo,
            VK_NULL_HANDLE),
            "Failed to submit queue!"
        );
    }

    u64 Timeline::GetTimelineValue(usize frameIndex, TimelineStage timelineStage) const
    {
        // Since we use an initial value of 0, an easy fix is to add 1 to the frame index
        // 0 -> 1 * TIMELINE_STAGE_COUNT + 0 -> ....
        return (frameIndex + 1) * TimelineStage::TIMELINE_STAGE_COUNT + timelineStage;
    }

    void Timeline::WaitForStage(usize frameIndex, TimelineStage timelineStage, VkDevice device) const
    {
        const u64 value = GetTimelineValue(frameIndex, timelineStage);

        const VkSemaphoreWaitInfo waitInfo =
        {
            .sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
            .pNext          = nullptr,
            .flags          = 0,
            .semaphoreCount = 1,
            .pSemaphores    = &semaphore,
            .pValues        = &value
        };

        Vk::CheckResult(vkWaitSemaphores(
            device,
            &waitInfo,
            std::numeric_limits<u64>::max()),
            "Failed to wait for semaphore!"
        );
    }

    bool Timeline::IsAtOrPastState(usize frameIndex, TimelineStage timelineStage, VkDevice device) const
    {
        const u64 value = GetTimelineValue(frameIndex, timelineStage);

        u64 currentValue = 0;
        Vk::CheckResult(vkGetSemaphoreCounterValue(
            device,
            semaphore,
            &currentValue),
            "Failed to get semaphore counter value!"
        );

        return currentValue >= value;
    }

    void Timeline::Destroy(VkDevice device)
    {
        vkDestroySemaphore(device, semaphore, nullptr);
    }
}
