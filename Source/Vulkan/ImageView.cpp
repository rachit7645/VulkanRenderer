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

#include "ImageView.h"

#include <volk/volk.h>

#include "Util/Log.h"
#include "Util.h"

namespace Vk
{
    ImageView::ImageView
    (
        VkDevice device,
        const Vk::Image& image,
        VkImageViewType viewType,
        VkFormat format,
        const VkImageSubresourceRange& subresourceRange
    )
    {
        const VkImageViewCreateInfo createInfo =
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
            .subresourceRange = subresourceRange
        };

        Vk::CheckResult(vkCreateImageView(
            device,
            &createInfo,
            nullptr,
            &handle),
            "Failed to create image view!"
        );
    }

    bool ImageView::operator==(const ImageView& rhs) const
    {
        return handle == rhs.handle;
    }

    void ImageView::Destroy(VkDevice device) const
    {
        if (handle == VK_NULL_HANDLE)
        {
            return;
        }

        Logger::Debug("Destroying image view! [handle={}]\n", std::bit_cast<void*>(handle));

        vkDestroyImageView(device, handle, nullptr);
    }
}