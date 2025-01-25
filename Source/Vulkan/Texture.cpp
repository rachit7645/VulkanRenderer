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
    Vk::Buffer Texture::LoadFromFile
    (
        VkDevice device,
        VmaAllocator allocator,
        VkFormat format,
        const std::string_view path,
        Flags flags
    )
    {
        this->flags = flags;

        const auto         imageData = STB::Image(path, vkuFormatHasAlpha(format) ? STBI_rgb_alpha : STBI_rgb);
        const VkDeviceSize imageSize = imageData.width * imageData.height * imageData.channels;

        const auto stagingBuffer = Vk::Buffer
        (
            allocator,
            imageSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            VMA_MEMORY_USAGE_AUTO
        );

        std::memcpy(stagingBuffer.allocInfo.pMappedData, imageData.data, imageSize);

        const auto mipLevels = IsFlagSet(flags, Flags::GenMipmaps) ?
                         static_cast<u32>(std::floor(std::log2(std::max(imageData.width, imageData.height)))) + 1 :
                         1;

        image = Vk::Image
        (
            allocator,
            imageData.width,
            imageData.height,
            mipLevels,
            format,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
        );

        imageView = Vk::ImageView
        (
            device,
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

        const auto name = Engine::Files::GetNameWithoutExtension(path);

        Vk::SetDebugName(device, image.handle,     name);
        Vk::SetDebugName(device, imageView.handle, name + "_View");

        Logger::Debug("Loaded texture! [Path={}]\n", path);

        return stagingBuffer;
    }

    void Texture::UploadToGPU(const Vk::CommandBuffer& cmdBuffer, const Vk::Buffer& stagingBuffer)
    {
        const VkDeviceSize imageSize = image.width * image.height * vkuFormatComponentCount(image.format);

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
            // Mipmap generation would have changed the layout automatically
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

    bool Texture::operator==(const Texture& rhs) const
    {
        return image == rhs.image && imageView == rhs.imageView;
    }

    bool Texture::IsFlagSet(Texture::Flags combined, Texture::Flags flag)
    {
        return (combined & flag) == flag;
    }

    void Texture::Destroy(VkDevice device, VmaAllocator allocator) const
    {
        imageView.Destroy(device);
        image.Destroy(allocator);
    }
}