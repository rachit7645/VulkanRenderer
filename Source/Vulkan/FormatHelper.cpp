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

#include "FormatHelper.h"

#include "Util/Log.h"

namespace Vk
{
    FormatHelper::FormatHelper(VkPhysicalDevice physicalDevice)
    {
        colorAttachmentFormatLDR = FindSupportedFormat
        (
            physicalDevice,
            std::array{VK_FORMAT_R8G8B8A8_UNORM},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BIT |
            VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BLEND_BIT |
            VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT |
            VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_FILTER_LINEAR_BIT
        );

        colorAttachmentFormatHDR = FindSupportedFormat
        (
            physicalDevice,
            std::array
            {
                VK_FORMAT_B10G11R11_UFLOAT_PACK32,
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

        colorAttachmentFormatHDRWithAlpha = FindSupportedFormat
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

        depthFormat = FindSupportedFormat
        (
            physicalDevice,
            std::array
            {
                VK_FORMAT_D32_SFLOAT,
                VK_FORMAT_D32_SFLOAT_S8_UINT,
                VK_FORMAT_D24_UNORM_S8_UINT,
                VK_FORMAT_X8_D24_UNORM_PACK32,
                VK_FORMAT_D16_UNORM,
                VK_FORMAT_D16_UNORM_S8_UINT,
            },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT |
            VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT
        );
    }

    VkFormat FormatHelper::FindSupportedFormat
    (
        VkPhysicalDevice physicalDevice,
        const std::span<const VkFormat> candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags2 features
    )
    {
        for (const auto format : candidates)
        {
            VkFormatProperties3 properties3 = {};
            properties3.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3;
            properties3.pNext = nullptr;

            VkFormatProperties2 properties2 = {};
            properties2.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
            properties2.pNext = &properties3;

            vkGetPhysicalDeviceFormatProperties2(physicalDevice, format, &properties2);

            const bool isValidLinear  = (tiling == VK_IMAGE_TILING_LINEAR)  && ((properties3.linearTilingFeatures  & features) == features);
            const bool isValidOptimal = (tiling == VK_IMAGE_TILING_OPTIMAL) && ((properties3.optimalTilingFeatures & features) == features);

            if (isValidLinear || isValidOptimal)
            {
                return format;
            }
        }

        // No format was suitable, exit
        Logger::Error
        (
            "No valid formats found! [physicalDevice={}] [tiling={}] [features={}]\n",
            std::bit_cast<void*>(physicalDevice),
            string_VkImageTiling(tiling),
            string_VkFormatFeatureFlags(features)
        );
    }
}
