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

#include "ComputeTimeline.h"

#include "Util.h"
#include "DebugUtils.h"

namespace Vk
{
    ComputeTimeline::ComputeTimeline(VkDevice device)
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

        Vk::SetDebugName(device, semaphore, "Compute/TimelineSemaphore");
    }

    u64 ComputeTimeline::GetTimelineValue(usize frameIndex, ComputeTimelineStage timelineStage) const
    {
        // Since we use an initial value of 0, an easy fix is to add 1 to the frame index
        // 0 -> 1 * TIMELINE_STAGE_COUNT + 0 -> ....
        return (frameIndex + 1) * ComputeTimelineStage::COMPUTE_TIMELINE_STAGE_COUNT + timelineStage;
    }

    void ComputeTimeline::WaitForStage(usize frameIndex, ComputeTimelineStage timelineStage, VkDevice device) const
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

    bool ComputeTimeline::IsAtOrPastState(usize frameIndex, ComputeTimelineStage timelineStage, VkDevice device) const
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

    void ComputeTimeline::Destroy(VkDevice device)
    {
        vkDestroySemaphore(device, semaphore, nullptr);
    }
}
