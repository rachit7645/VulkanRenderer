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

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <vulkan/utility/vk_format_utils.h>
#include <volk/volk.h>

#include "Texture.h"

#include "Util.h"
#include "Buffer.h"
#include "DebugUtils.h"
#include "Util/Log.h"
#include "Util/Enum.h"
#include "Util/SIMD.h"

namespace Vk
{
    Texture::Upload Texture::LoadFromFile
    (
        VkDevice device,
        VmaAllocator allocator,
        const std::string_view path
    )
    {
        ktxTexture2* pTexture = nullptr;

        auto result = ktxTexture2_CreateFromNamedFile
        (
            path.data(),
            KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT | KTX_TEXTURE_CREATE_CHECK_GLTF_BASISU_BIT,
            &pTexture
        );

        if (result != KTX_SUCCESS)
        {
            Logger::Error("Failed to load KTX2 file! [Error={}] [Path={}]", ktxErrorString(result), path);
        }

        if (ktxTexture2_NeedsTranscoding(pTexture))
        {
            result = ktxTexture2_TranscodeBasis(pTexture, KTX_TTF_BC7_RGBA, 0);

            if (result != KTX_SUCCESS)
            {
                Logger::Error("Failed to transcode KTX2 file! [Error={}] [Path={}]", ktxErrorString(result), path);
            }
        }

        const auto stagingBuffer = Vk::Buffer
        (
            allocator,
            pTexture->dataSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            VMA_MEMORY_USAGE_AUTO
        );

        std::memcpy(stagingBuffer.allocInfo.pMappedData, pTexture->pData, pTexture->dataSize);

        std::vector<VkBufferImageCopy2> copyRegions = {};
        copyRegions.reserve(pTexture->numLevels);

        for (u32 mipLevel = 0; mipLevel < pTexture->numLevels; ++mipLevel)
        {
            ktx_size_t offset;
            ktxTexture_GetImageOffset(reinterpret_cast<ktxTexture*>(pTexture), mipLevel, 0, 0, &offset);

            copyRegions.emplace_back(VkBufferImageCopy2{
                .sType             = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
                .pNext             = nullptr,
                .bufferOffset      = offset,
                .bufferRowLength   = 0,
                .bufferImageHeight = 0,
                .imageSubresource  = {
                    .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel       = mipLevel,
                    .baseArrayLayer = 0,
                    .layerCount     = 1
                },
                .imageOffset = {0, 0, 0},
                .imageExtent = {pTexture->baseWidth >> mipLevel, pTexture->baseHeight >> mipLevel, 1}
            });
        }

        image = Vk::Image
        (
            allocator,
            {
                .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext                 = nullptr,
                .flags                 = 0,
                .imageType             = VK_IMAGE_TYPE_2D,
                .format                = static_cast<VkFormat>(pTexture->vkFormat),
                .extent                = {pTexture->baseWidth, pTexture->baseHeight, 1},
                .mipLevels             = pTexture->numLevels,
                .arrayLayers           = 1,
                .samples               = VK_SAMPLE_COUNT_1_BIT,
                .tiling                = VK_IMAGE_TILING_OPTIMAL,
                .usage                 = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 0,
                .pQueueFamilyIndices   = nullptr,
                .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED
            },
            VK_IMAGE_ASPECT_COLOR_BIT
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

        ktxTexture_Destroy(reinterpret_cast<ktxTexture*>(pTexture));

        const auto name = Engine::Files::GetNameWithoutExtension(path);

        Vk::SetDebugName(device, image.handle,     name);
        Vk::SetDebugName(device, imageView.handle, name + "_View");

        Logger::Debug("Loaded texture! [Path={}]\n", path);

        return {stagingBuffer, std::move(copyRegions)};
    }

    Texture::Upload Texture::LoadFromFileHDR
    (
        VkDevice device,
        VmaAllocator allocator,
        VkFormat format,
        const std::string_view path
    )
    {
        s32 width    = 0;
        s32 height   = 0;
        s32 channels = 0;

        const f32* data = stbi_loadf
        (
            path.data(),
            &width,
            &height,
            &channels,
            STBI_rgb_alpha
        );

        if (data == nullptr)
        {
            Logger::Error("Unable to load texture! [Path={}]\n", path);
        }

        const usize        elemCount = width * height * STBI_rgb_alpha;
        const VkDeviceSize imageSize = elemCount * sizeof(f16);

        const auto stagingBuffer = Vk::Buffer
        (
            allocator,
            imageSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            VMA_MEMORY_USAGE_AUTO
        );

        Util::ConvertF32ToF16(data, std::bit_cast<f16*>(stagingBuffer.allocInfo.pMappedData), elemCount);

        stbi_image_free(std::bit_cast<void*>(data));

        std::vector<VkBufferImageCopy2> copyRegions = {};

        copyRegions.emplace_back(VkBufferImageCopy2{
            .sType             = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
            .pNext             = nullptr,
            .bufferOffset      = 0,
            .bufferRowLength   = 0,
            .bufferImageHeight = 0,
            .imageSubresource  = {
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel       = 0,
                .baseArrayLayer = 0,
                .layerCount     = 1
            },
            .imageOffset = {0, 0, 0},
            .imageExtent = {static_cast<u32>(width), static_cast<u32>(height), 1}
        });

        image = Vk::Image
        (
            allocator,
            {
                .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext                 = nullptr,
                .flags                 = 0,
                .imageType             = VK_IMAGE_TYPE_2D,
                .format                = format,
                .extent                = {static_cast<u32>(width), static_cast<u32>(height), 1},
                .mipLevels             = 1,
                .arrayLayers           = 1,
                .samples               = VK_SAMPLE_COUNT_1_BIT,
                .tiling                = VK_IMAGE_TILING_OPTIMAL,
                .usage                 = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 0,
                .pQueueFamilyIndices   = nullptr,
                .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED
            },
            VK_IMAGE_ASPECT_COLOR_BIT
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

        return {stagingBuffer, std::move(copyRegions)};
    }

    Texture::Upload Texture::LoadFromMemory
    (
        VkDevice device,
        VmaAllocator allocator,
        VkFormat format,
        const std::span<const u8> data,
        const glm::uvec2 size
    )
    {
        const auto stagingBuffer = Vk::Buffer
        (
            allocator,
            data.size_bytes(),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            VMA_MEMORY_USAGE_AUTO
        );

        std::memcpy(stagingBuffer.allocInfo.pMappedData, data.data(), data.size_bytes());

        std::vector<VkBufferImageCopy2> copyRegions = {};

        copyRegions.emplace_back(VkBufferImageCopy2{
            .sType             = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
            .pNext             = nullptr,
            .bufferOffset      = 0,
            .bufferRowLength   = 0,
            .bufferImageHeight = 0,
            .imageSubresource  = {
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel       = 0,
                .baseArrayLayer = 0,
                .layerCount     = 1
            },
            .imageOffset       = {0, 0, 0},
            .imageExtent       = {size.x, size.y, 1}
        });

        image = Vk::Image
        (
            allocator,
            {
                .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext                 = nullptr,
                .flags                 = 0,
                .imageType             = VK_IMAGE_TYPE_2D,
                .format                = format,
                .extent                = {size.x, size.y, 1},
                .mipLevels             = 1,
                .arrayLayers           = 1,
                .samples               = VK_SAMPLE_COUNT_1_BIT,
                .tiling                = VK_IMAGE_TILING_OPTIMAL,
                .usage                 = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 0,
                .pQueueFamilyIndices   = nullptr,
                .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED
            },
            VK_IMAGE_ASPECT_COLOR_BIT
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

        return {stagingBuffer, std::move(copyRegions)};
    }

    void Texture::UploadToGPU(const Vk::CommandBuffer& cmdBuffer, const Upload& upload)
    {
        const auto& [stagingBuffer, copyRegions] = upload;

        stagingBuffer.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_HOST_BIT,
            VK_ACCESS_2_HOST_WRITE_BIT,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_READ_BIT,
            0,
            VK_WHOLE_SIZE
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

        const VkCopyBufferToImageInfo2 copyInfo =
        {
            .sType          = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
            .pNext          = nullptr,
            .srcBuffer      = stagingBuffer.handle,
            .dstImage       = image.handle,
            .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .regionCount    = static_cast<u32>(copyRegions.size()),
            .pRegions       = copyRegions.data()
        };

        vkCmdCopyBufferToImage2(cmdBuffer.handle, &copyInfo);

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

    bool Texture::operator==(const Texture& rhs) const
    {
        return image == rhs.image && imageView == rhs.imageView;
    }

    void Texture::Destroy(VkDevice device, VmaAllocator allocator) const
    {
        imageView.Destroy(device);
        image.Destroy(allocator);
    }
}