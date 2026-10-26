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

#ifndef COMPUTE_TIMELINE_H
#define COMPUTE_TIMELINE_H

#include <vulkan/vulkan.h>

#include "Util/Types.h"

namespace Vk
{
    class ComputeTimeline
    {
    public:
        enum class Stage : u64
        {
            AsyncComputeFinished = 0,
            Count
        };

        explicit ComputeTimeline(VkDevice device);

        [[nodiscard]] u64 GetTimelineValue(usize frameIndex, ComputeTimeline::Stage timelineStage) const;

        void Destroy(VkDevice device);

        VkSemaphore semaphore = VK_NULL_HANDLE;
    };
}

#endif
