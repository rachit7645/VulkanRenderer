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

#include <vulkan/utility/vk_format_utils.h>
#include <volk/volk.h>

#include "Texture.h"
#include "Util.h"
#include "Buffer.h"
#include "DebugUtils.h"
#include "Util/Log.h"
#include "Util/Enum.h"
#include "Externals/STBImage.h"

namespace Vk
{
    Texture::Texture
    (
        const Vk::Context& context,
        const Vk::Buffer& stagingBuffer,
        const std::string_view path,
        Flags flags
    )
    {
        auto candidates = IsFlagSet(flags, Flags::IsSRGB) ?
                          std::array{VK_FORMAT_R8G8B8_SRGB, VK_FORMAT_R8G8B8A8_SRGB} :
                          std::array{VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_R8G8B8A8_UNORM};

        auto format = Vk::FindSupportedFormat
        (
            context.physicalDevice,
            candidates,
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_2_TRANSFER_DST_BIT  |
            VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT |
            VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_FILTER_LINEAR_BIT
        );

        auto components = STBI_rgb;
        if (vkuFormatHasAlpha(format))
        {
            // TONS of wasted space but ehh
            components = STBI_rgb_alpha;
        }

        auto         imageData = STB::Image(path, components);
        VkDeviceSize imageSize = imageData.width * imageData.height * components;

        if (imageSize > stagingBuffer.allocInfo.size)
        {
            Logger::Error
            (
                "Not enough space in staging buffer! [ImageSize={}] [BufferSize={}]\n",
                imageSize,
                stagingBuffer.allocInfo.size
            );
        }

        std::memcpy(stagingBuffer.allocInfo.pMappedData, imageData.data, imageSize);

        auto mipLevels = IsFlagSet(flags, Flags::GenMipmaps) ?
                         static_cast<u32>(std::floor(std::log2(std::max(imageData.width, imageData.height)))) + 1 :
                         1;

        image = Vk::Image
        (
            context.allocator,
            imageData.width,
            imageData.height,
            mipLevels,
            format,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
        );

        // Transfer
        Vk::ImmediateSubmit
        (
            context.device,
            context.graphicsQueue,
            context.commandPool,
            [&](const Vk::CommandBuffer& cmdBuffer)
            {
                stagingBuffer.Barrier
                (
                    cmdBuffer,
                    VK_PIPELINE_STAGE_2_HOST_BIT,
                    VK_ACCESS_2_HOST_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    VK_ACCESS_2_TRANSFER_READ_BIT,
                    0,
                    imageSize
                );

                image.Barrier
                (
                    cmdBuffer,
                    VK_PIPELINE_STAGE_2_NONE,
                    VK_ACCESS_2_NONE,
                    VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    {
                        .aspectMask     = image.aspect,
                        .baseMipLevel   = 0,
                        .levelCount     = image.mipLevels,
                        .baseArrayLayer = 0,
                        .layerCount     = 1
                    }
                );

                VkBufferImageCopy2 copyRegion =
                {
                    .sType             = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
                    .pNext             = nullptr,
                    .bufferOffset      = 0,
                    .bufferRowLength   = 0,
                    .bufferImageHeight = 0,
                    .imageSubresource  = {
                        .aspectMask     = image.aspect,
                        .mipLevel       = 0,
                        .baseArrayLayer = 0,
                        .layerCount     = 1
                    },
                    .imageOffset       = {0, 0, 0},
                    .imageExtent       = {image.width, image.height, 1}
                };

                VkCopyBufferToImageInfo2 copyInfo =
                {
                    .sType          = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
                    .pNext          = nullptr,
                    .srcBuffer      = stagingBuffer.handle,
                    .dstImage       = image.handle,
                    .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .regionCount    = 1,
                    .pRegions       = &copyRegion
                };

                vkCmdCopyBufferToImage2(cmdBuffer.handle, &copyInfo);

                if (IsFlagSet(flags, Flags::GenMipmaps))
                {
                    image.GenerateMipmaps(cmdBuffer);
                }
                else
                {
                    // Mipmap generation would change layout automatically
                    image.Barrier
                    (
                        cmdBuffer,
                        VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                        VK_ACCESS_2_TRANSFER_WRITE_BIT,
                        VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                        VK_ACCESS_2_SHADER_READ_BIT,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        {
                            .aspectMask     = image.aspect,
                            .baseMipLevel   = 0,
                            .levelCount     = image.mipLevels,
                            .baseArrayLayer = 0,
                            .layerCount     = 1
                        }
                    );
                }
            }
        );

        imageView = Vk::ImageView
        (
            context.device,
            image,
            VK_IMAGE_VIEW_TYPE_2D,
            image.format,
            {
                .aspectMask     = image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        auto name = Engine::Files::GetNameWithoutExtension(path);

        Vk::SetDebugName(context.device, image.handle,     name);
        Vk::SetDebugName(context.device, imageView.handle, name + "_View");

        Logger::Debug("Loaded texture! [Path={}]\n", path);
    }

    bool Texture::operator==(const Texture& rhs) const
    {
        return image == rhs.image && imageView == rhs.imageView;
    }

    bool Texture::IsFlagSet(Texture::Flags combined, Texture::Flags flag) const
    {
        return (combined & flag) == flag;
    }

    void Texture::Destroy(VkDevice device, VmaAllocator allocator) const
    {
        imageView.Destroy(device);
        image.Destroy(allocator);
    }
}