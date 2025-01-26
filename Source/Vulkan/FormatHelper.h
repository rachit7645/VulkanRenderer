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

        VkFormat textureFormat;
        VkFormat textureFormatSRGB;
        VkFormat colorAttachmentFormatHDR;
        VkFormat depthFormat;
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
