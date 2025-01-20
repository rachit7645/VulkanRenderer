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

#include "DepthBuffer.h"

#include <vulkan/utility/vk_format_utils.h>

#include "Util.h"
#include "Util/Log.h"

namespace Vk
{
    DepthBuffer::DepthBuffer(const Vk::Context& context, VkExtent2D swapchainExtent)
    {
        const auto depthFormat = GetDepthFormat(context.physicalDevice);
        const bool hasStencil  = vkuFormatHasStencil(depthFormat);

        depthImage = Vk::Image
        (
            context.allocator,
            swapchainExtent.width,
            swapchainExtent.height,
            1,
            depthFormat,
            VK_IMAGE_TILING_OPTIMAL,
            hasStencil ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
        );

        depthImageView = Vk::ImageView
        (
            context.device,
            depthImage,
            VK_IMAGE_VIEW_TYPE_2D,
            depthImage.format,
            static_cast<VkImageAspectFlagBits>(depthImage.aspect),
            0,
            1,
            0,
            1
        );

        Vk::ImmediateSubmit(context, [&](const Vk::CommandBuffer& cmdBuffer)
        {
            depthImage.Barrier
            (
                cmdBuffer,
                VK_PIPELINE_STAGE_2_NONE,
                VK_ACCESS_2_NONE,
                VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                {
                    .aspectMask     = depthImage.aspect,
                    .baseMipLevel   = 0,
                    .levelCount     = depthImage.mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount     = 1
                }
            );
        });
    }

    VkFormat DepthBuffer::GetDepthFormat(VkPhysicalDevice physicalDevice)
    {
        return Vk::FindSupportedFormat
        (
            physicalDevice,
            std::array<VkFormat, 5>
            {
                VK_FORMAT_D32_SFLOAT,
                VK_FORMAT_D24_UNORM_S8_UINT,
                VK_FORMAT_D32_SFLOAT_S8_UINT,
                VK_FORMAT_D16_UNORM,
                VK_FORMAT_D16_UNORM_S8_UINT,
            },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_2_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }

    void DepthBuffer::Destroy(VkDevice device, VmaAllocator allocator) const
    {
        depthImageView.Destroy(device);
        depthImage.Destroy(allocator);
    }
}