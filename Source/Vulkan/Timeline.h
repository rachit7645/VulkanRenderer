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

#ifndef TIMELINE_H
#define TIMELINE_H

#include <vulkan/vulkan.h>

#include "Util/Util.h"

namespace Vk
{
    class Timeline
    {
    public:
        enum TimelineStage : u64
        {
            TIMELINE_STAGE_SWAPCHAIN_IMAGE_ACQUIRED = 0,
            TIMELINE_STAGE_RENDER_FINISHED         = 1,
            TIMELINE_STAGE_COUNT
        };

        explicit Timeline(VkDevice device);

        void AcquireImageToTimeline(usize frameIndex, VkQueue queue, VkSemaphore imageAcquire);
        void TimelineToRenderFinished(usize frameIndex, VkQueue queue, VkSemaphore renderFinished);

        u64 GetTimelineValue(usize frameIndex, TimelineStage timelineStage);

        void WaitForStage(usize frameIndex, TimelineStage timelineStage, VkDevice device);

        void Destroy(VkDevice device);

        VkSemaphore semaphore = VK_NULL_HANDLE;
    };
}

#endif
