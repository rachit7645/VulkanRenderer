/*
 *    Copyright 2023 - 2024 Rachit Khandelwal
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
#include "Buffer.h"
#include "Util/Log.h"
#include "Util/Enum.h"
#include "Externals/STBImage.h"

namespace Vk
{
    Texture::Texture(const Vk::Context& context, const std::string_view path, Texture::Flags flags)
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

        auto stagingBuffer = Vk::Buffer
        (
            context.allocator,
            imageSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            VMA_MEMORY_USAGE_AUTO
        );

        stagingBuffer.Map(context.allocator);
            std::memcpy(stagingBuffer.allocInfo.pMappedData, imageData.data, imageSize);
        stagingBuffer.Unmap(context.allocator);

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
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        // Transition layout for transfer
        Vk::ImmediateSubmit(context, [&](const Vk::CommandBuffer& cmdBuffer)
        {
            image.TransitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        });

        image.CopyFromBuffer(context, stagingBuffer);

        stagingBuffer.Destroy(context.allocator);

        imageView = Vk::ImageView
        (
            context.device,
            image,
            VK_IMAGE_VIEW_TYPE_2D,
            image.format,
            image.aspect,
            0,
            image.mipLevels,
            0,
            1
        );

        if (IsFlagSet(flags, Flags::GenMipmaps))
        {
            image.GenerateMipmaps(context);
        }

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