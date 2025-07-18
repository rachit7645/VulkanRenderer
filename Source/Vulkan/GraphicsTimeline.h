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

#ifndef GRAPHICS_TIMELINE_H
#define GRAPHICS_TIMELINE_H

#include <vulkan/vulkan.h>

#include "Util/Types.h"

namespace Vk
{
    class GraphicsTimeline
    {
    public:
        enum GraphicsTimelineStage : u64
        {
            GRAPHICS_TIMELINE_STAGE_SWAPCHAIN_IMAGE_ACQUIRED    = 0,
            GRAPHICS_TIMELINE_STAGE_GBUFFER_GENERATION_COMPLETE = 1,
            GRAPHICS_TIMELINE_STAGE_RAY_DISPATCH                = 2,
            GRAPHICS_TIMELINE_STAGE_RENDER_FINISHED             = 3,
            GRAPHICS_TIMELINE_STAGE_COUNT
        };

        explicit GraphicsTimeline(VkDevice device);

        void AcquireImageToTimeline(usize frameIndex, VkQueue queue, VkSemaphore imageAcquire);
        void TimelineToRenderFinished(usize frameIndex, VkQueue queue, VkSemaphore renderFinished);

        [[nodiscard]] u64 GetTimelineValue(usize frameIndex, GraphicsTimelineStage timelineStage) const;

        void WaitForStage(usize frameIndex, GraphicsTimelineStage timelineStage, VkDevice device) const;
        bool IsAtOrPastState(usize frameIndex, GraphicsTimelineStage timelineStage, VkDevice device) const;

        void Destroy(VkDevice device);

        VkSemaphore semaphore = VK_NULL_HANDLE;
    };
}

#endif
