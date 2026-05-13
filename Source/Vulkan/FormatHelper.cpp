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

#include "FormatHelper.h"

#include <array>

#include "Util.h"

namespace Vk
{
    FormatHelper::FormatHelper(VkPhysicalDevice physicalDevice)
    {
        colorAttachmentFormatLDR = Vk::FindSupportedFormat
        (
            physicalDevice,
            std::array{VK_FORMAT_R8G8B8A8_UNORM},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BIT |
            VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BLEND_BIT |
            VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT |
            VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_FILTER_LINEAR_BIT
        );

        colorAttachmentFormatHDR = Vk::FindSupportedFormat
        (
            physicalDevice,
            std::array
            {
                VK_FORMAT_R16G16B16A16_SFLOAT,
                VK_FORMAT_R32G32B32A32_SFLOAT,
                VK_FORMAT_R64G64B64A64_SFLOAT
            },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BIT |
            VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BLEND_BIT |
            VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT |
            VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_FILTER_LINEAR_BIT
        );

        depthFormat = Vk::FindSupportedFormat
        (
            physicalDevice,
            std::array
            {
                VK_FORMAT_D32_SFLOAT,
                VK_FORMAT_D32_SFLOAT_S8_UINT
            },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT |
            VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT
        );
    }
}