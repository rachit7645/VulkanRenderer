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

#ifndef GRAPHICS_TIMELINE_H
#define GRAPHICS_TIMELINE_H

#include <vulkan/vulkan.h>

#include "Util/Types.h"

namespace Vk
{
    class GraphicsTimeline
    {
    public:
        enum class Stage : u64
        {
            SwapchainImageAcquired    = 0,
            GbufferGenerationComplete = 1,
            RayDispatch               = 2,
            RenderFinished            = 3,
            Count
        };

        explicit GraphicsTimeline(VkDevice device);

        void AcquireImageToTimeline(usize frameIndex, VkQueue queue, VkSemaphore imageAcquire);
        void TimelineToRenderFinished(usize frameIndex, VkQueue queue, VkSemaphore renderFinished);

        [[nodiscard]] u64 GetTimelineValue(usize frameIndex, GraphicsTimeline::Stage timelineStage) const;

        void WaitForStage(usize frameIndex, GraphicsTimeline::Stage timelineStage, VkDevice device) const;
        bool IsAtOrPastStage(usize frameIndex, GraphicsTimeline::Stage timelineStage, VkDevice device) const;

        void Destroy(VkDevice device);

        VkSemaphore semaphore = VK_NULL_HANDLE;
    };
}

#endif
