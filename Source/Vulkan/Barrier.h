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

#ifndef BARRIER_H
#define BARRIER_H

#include "Util/Util.h"

namespace Vk
{
    struct BufferBarrier
    {
        VkPipelineStageFlags2 srcStageMask  = VK_PIPELINE_STAGE_2_NONE;
        VkAccessFlags2        srcAccessMask = VK_ACCESS_2_NONE;
        VkPipelineStageFlags2 dstStageMask  = VK_PIPELINE_STAGE_2_NONE;
        VkAccessFlags2        dstAccessMask = VK_ACCESS_2_NONE;
        VkDeviceSize          offset        = 0;
        VkDeviceSize          size          = VK_WHOLE_SIZE;
    };

    struct ImageBarrier
    {
        VkPipelineStageFlags2 srcStageMask   = VK_PIPELINE_STAGE_2_NONE;
        VkAccessFlags2        srcAccessMask  = VK_ACCESS_2_NONE;
        VkPipelineStageFlags2 dstStageMask   = VK_PIPELINE_STAGE_2_NONE;
        VkAccessFlags2        dstAccessMask  = VK_ACCESS_2_NONE;
        VkImageLayout         oldLayout      = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout         newLayout      = VK_IMAGE_LAYOUT_UNDEFINED;
        u32                   baseMipLevel   = 0;
        u32                   levelCount     = VK_REMAINING_MIP_LEVELS;
        u32                   baseArrayLayer = 0;
        u32                   layerCount     = VK_REMAINING_ARRAY_LAYERS;
    };
}

#endif
