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
        // Default constructor
        ImageView() = default;

        // Create image view
        ImageView
        (
            VkDevice device,
            const Vk::Image& image,
            VkImageViewType viewType,
            VkFormat format,
            VkImageAspectFlagBits aspectMask,
            u32 baseMipLevel,
            u32 levelCount,
            u32 baseArrayLayer,
            u32 layerCount
        );

        // Equality operator
        bool operator==(const ImageView& rhs) const;

        // Destroy view
        void Destroy(VkDevice device) const;

        // Vulkan handle
        VkImageView handle = VK_NULL_HANDLE;
    };
}

// Don't nuke me for this
namespace std
{
    // Hashing
    template <>
    struct hash<Vk::ImageView>
    {
        std::size_t operator()(const Vk::ImageView& imageView) const
        {
            // Just return the hash of the handle
            return std::hash<VkImageView>()(imageView.handle);
        }
    };
}

#endif
