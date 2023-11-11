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

#ifndef IMAGE_VIEW_H
#define IMAGE_VIEW_H

#include <vulkan/vulkan.h>
#include "Image.h"
#include "Util/Util.h"

namespace Vk
{
    class ImageView
    {
    public:
        // Create image view
        ImageView
        (
            VkDevice device,
            const Vk::Image& image,
            VkImageViewType viewType,
            VkFormat format,
            VkImageAspectFlagBits aspectMask,
            u32 baseMipLevel = 0,
            u32 levelCount = 1,
            u32 baseArrayLayer = 0,
            u32 layerCount = 1
        );
        // Vulkan handle
        VkImageView handle = VK_NULL_HANDLE;
    };
}

#endif
