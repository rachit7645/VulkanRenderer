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
    DepthBuffer::DepthBuffer(const Vk::Context& context, const Vk::FormatHelper& formatHelper, VkExtent2D extent)
    {
        depthImage = Vk::Image
        (
            context.allocator,
            {
                .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext                 = nullptr,
                .flags                 = 0,
                .imageType             = VK_IMAGE_TYPE_2D,
                .format                = formatHelper.depthFormat,
                .extent                = {extent.width, extent.height, 1},
                .mipLevels             = 1,
                .arrayLayers           = 1,
                .samples               = VK_SAMPLE_COUNT_1_BIT,
                .tiling                = VK_IMAGE_TILING_OPTIMAL,
                .usage                 = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 0,
                .pQueueFamilyIndices   = nullptr,
                .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED
            },
            vkuFormatHasStencil(formatHelper.depthFormat)
                ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT
                : VK_IMAGE_ASPECT_DEPTH_BIT
        );

        depthImageView = Vk::ImageView
        (
            context.device,
            depthImage,
            VK_IMAGE_VIEW_TYPE_2D,
            depthImage.format,
            {
                .aspectMask     = depthImage.aspect,
                .baseMipLevel   = 0,
                .levelCount     = depthImage.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        Vk::ImmediateSubmit
        (
            context.device,
            context.graphicsQueue,
            context.commandPool,
            [&](const Vk::CommandBuffer& cmdBuffer)
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
            }
        );
    }

    void DepthBuffer::Destroy(VkDevice device, VmaAllocator allocator) const
    {
        depthImageView.Destroy(device);
        depthImage.Destroy(allocator);
    }
}