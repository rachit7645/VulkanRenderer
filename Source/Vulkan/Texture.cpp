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

#include <vulkan/utility/vk_format_utils.h>

#include "Texture.h"
#include "Util.h"
#include "Util/Log.h"
#include "Buffer.h"

namespace Vk
{
    Texture::Texture(const std::shared_ptr<Vk::Context>& context, const std::string_view path, Texture::Flags flags)
    {
        // Log
        Logger::Info("Loading texture {}\n", path.data());

        // Format candidates
        auto candidates = (flags & Flags::IsSRGB) == Flags::IsSRGB ?
                                         std::array<VkFormat, 2>{VK_FORMAT_R8G8B8_SRGB, VK_FORMAT_R8G8B8A8_SRGB} :
                                         std::array<VkFormat, 2>{VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_R8G8B8A8_UNORM};

        // Get supported formats
        auto format = Vk::FindSupportedFormat
        (
            context->physicalDevice,
            candidates,
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_TRANSFER_DST_BIT  |
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
            VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT
        );

        // Get components
        auto components = STBI_rgb;
        // Check for formats with alpha
        if (vkuFormatHasAlpha(format))
        {
            // We have four components (TONS of wasted space but ehh)
            components = STBI_rgb_alpha;
        }

        // Load image
        auto imageData = STB::STBImage(path, components);
        // Image size
        VkDeviceSize imageSize = imageData.width * imageData.height * components;

        // Create staging buffer
        auto stagingBuffer = Vk::Buffer
        (
            context,
            imageSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );

        // Copy data
        stagingBuffer.LoadData
        (
            context->device,
            std::span(static_cast<const stbi_uc*>(imageData.data), imageSize / sizeof(stbi_uc))
        );

        // Mipmap levels
        auto mipLevels = (flags & Flags::GenMipmaps) == Flags::GenMipmaps ?
                              static_cast<u32>(std::floor(std::log2(std::max(imageData.width, imageData.height)))) + 1 :
                              1;
        // Create image
        image = Vk::Image
        (
            context,
            imageData.width,
            imageData.height,
            mipLevels,
            format,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        // Transition layout for transfer
        Vk::ImmediateSubmit(context, [&](const Vk::CommandBuffer& cmdBuffer)
        {
            image.TransitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        });
        // Copy data
        image.CopyFromBuffer(context, stagingBuffer);

        // Destroy staging buffer
        stagingBuffer.DeleteBuffer(context->device);

        // Create image view
        imageView = Vk::ImageView
        (
            context->device,
            image,
            VK_IMAGE_VIEW_TYPE_2D,
            image.format,
            static_cast<VkImageAspectFlagBits>(image.aspect),
            0,
            image.mipLevels
        );

        // Generate mipmaps
        GenerateMipmaps(context);
    }

    void Texture::GenerateMipmaps(const std::shared_ptr<Vk::Context>& context)
    {
        Vk::ImmediateSubmit(context, [&] (const Vk::CommandBuffer& cmdBuffer)
        {
            // Barrier
            VkImageMemoryBarrier barrier =
            {
                .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext               = nullptr,
                .srcAccessMask       = VK_ACCESS_NONE,
                .dstAccessMask       = VK_ACCESS_NONE,
                .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image               = image.handle,
                .subresourceRange    = {
                .aspectMask     = image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
                }
            };

            // Mip dimensions
            s32 mipWidth  = static_cast<s32>(image.width);
            s32 mipHeight = static_cast<s32>(image.height);

            // Loop over the rest of the mipmap chain
            for (u32 i = 1; i < image.mipLevels; ++i)
            {
                // Mipmap level
                barrier.subresourceRange.baseMipLevel = i - 1;
                // Layout
                barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                // Access mask
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

                // Barrier
                vkCmdPipelineBarrier
                (
                    cmdBuffer.handle,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &barrier
                );

                // Blit info
                VkImageBlit blitInfo =
                {
                    .srcSubresource =
                    {
                        .aspectMask     = image.aspect,
                        .mipLevel       = i - 1,
                        .baseArrayLayer = 0,
                        .layerCount     = 1
                    },
                    .srcOffsets = {
                        {0, 0, 0},
                        {mipWidth, mipHeight, 1}
                    },
                    .dstSubresource =
                    {
                        .aspectMask     = image.aspect,
                        .mipLevel       = i,
                        .baseArrayLayer = 0,
                        .layerCount     = 1
                    },
                    .dstOffsets =
                    {
                        {0, 0, 0},
                        {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1}
                    }
                };

                // Blit Image
                vkCmdBlitImage
                (
                    cmdBuffer.handle,
                    image.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    image.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1, &blitInfo,
                    VK_FILTER_LINEAR
                );

                // Configure layouts
                barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                // Configure access masks
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                // Barrier
                vkCmdPipelineBarrier
                (
                    cmdBuffer.handle,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &barrier
                );

                // Divide mip dimensions
                if (mipWidth > 1) mipWidth /= 2;
                if (mipHeight > 1) mipHeight /= 2;
            }

            // Base mipmap level
            barrier.subresourceRange.baseMipLevel = image.mipLevels - 1;
            // Configure layouts
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            // Configure access masks
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            // Transition final level
            vkCmdPipelineBarrier
            (
                cmdBuffer.handle,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );
        });
    }

    bool Texture::operator==(const Texture& rhs) const
    {
        // Return
        return image == rhs.image && imageView == rhs.imageView;
    }

    void Texture::Destroy(VkDevice device) const
    {
        // Destroy image view
        imageView.Destroy(device);
        // Destroy image
        image.Destroy(device);
    }
}