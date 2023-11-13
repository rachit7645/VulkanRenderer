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

#include "ImageView.h"
#include "Util/Log.h"

namespace Vk
{
    ImageView::ImageView
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
    )
    {
        // Image view creation data
        VkImageViewCreateInfo createInfo =
        {
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext            = nullptr,
            .flags            = 0,
            .image            = image.handle,
            .viewType         = viewType,
            .format           = format,
            .components       = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = {
                .aspectMask     = aspectMask,
                .baseMipLevel   = baseMipLevel,
                .levelCount     = levelCount,
                .baseArrayLayer = baseArrayLayer,
                .layerCount     = layerCount
            }
        };
        // Create image view
        if (vkCreateImageView(
                device,
                &createInfo,
                nullptr,
                &handle
            ) != VK_SUCCESS)
        {
            // Log
            Logger::Error
            (
                "Failed to create image view! [device={}] [image={}]",
                reinterpret_cast<void*>(device),
                reinterpret_cast<void*>(image.handle)
            );
        }
    }

    void ImageView::Destroy(VkDevice device)
    {
        // Destroy
        vkDestroyImageView(device, handle, nullptr);
    }
}