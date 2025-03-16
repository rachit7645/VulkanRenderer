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

#ifndef FORMAT_HELPER_H
#define FORMAT_HELPER_H

#include <vulkan/vulkan.h>

namespace Vk
{
    class FormatHelper
    {
    public:
        explicit FormatHelper(VkPhysicalDevice physicalDevice);

        VkFormat textureFormat                     = VK_FORMAT_UNDEFINED;
        VkFormat textureFormatSRGB                 = VK_FORMAT_UNDEFINED;
        VkFormat textureFormatBC7                  = VK_FORMAT_UNDEFINED;
        VkFormat textureFormatBC7SRGB              = VK_FORMAT_UNDEFINED;
        VkFormat textureFormatHDR                  = VK_FORMAT_UNDEFINED;
        VkFormat colorAttachmentFormatLDR          = VK_FORMAT_UNDEFINED;
        VkFormat colorAttachmentFormatHDR          = VK_FORMAT_UNDEFINED;
        VkFormat colorAttachmentFormatHDRWithAlpha = VK_FORMAT_UNDEFINED;
        VkFormat depthFormat                       = VK_FORMAT_UNDEFINED;
        VkFormat depthStencilFormat                = VK_FORMAT_UNDEFINED;
        VkFormat rNormFormat                       = VK_FORMAT_UNDEFINED;
        VkFormat rU8Format                         = VK_FORMAT_UNDEFINED;
        VkFormat rU16Format                        = VK_FORMAT_UNDEFINED;
        VkFormat rgNormFormat                      = VK_FORMAT_UNDEFINED;
        VkFormat rgFloatFormat                     = VK_FORMAT_UNDEFINED;
    private:
        VkFormat FindSupportedFormat
        (
            VkPhysicalDevice physicalDevice,
            const std::span<const VkFormat> candidates,
            VkImageTiling tiling,
            VkFormatFeatureFlags2 features
        );
    };
}

#endif