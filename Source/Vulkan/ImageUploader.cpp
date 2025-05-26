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

#include "ImageUploader.h"

#include <ktx.h>
#include <stb/stb_image.h>
#include <vulkan/utility/vk_format_utils.h>

#include "Util/Log.h"
#include "Util/SIMD.h"
#include "Externals/Tracy.h"

namespace Vk
{
    Vk::Image ImageUploader::LoadImageFromFile
    (
        VmaAllocator allocator,
        Util::DeletionQueue& deletionQueue,
        const std::string_view path
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneNamed(zone, true);
        zone.NameFmt("%s", std::string(Util::Files::GetName(path)).c_str());
        #endif

        const auto extension = Util::Files::GetExtension(path);

        if (extension == ".ktx2")
        {
            return LoadImageKTX2(allocator, deletionQueue, path);
        }

        if (extension == ".hdr")
        {
            return LoadImageHDR(allocator, deletionQueue, path);
        }

        return LoadImageSTBI(allocator, deletionQueue, path);
    }

    Vk::Image ImageUploader::LoadImageKTX2
    (
        VmaAllocator allocator,
        Util::DeletionQueue& deletionQueue,
        const std::string_view path
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScopedN("Khronos KTX2 Loader");
        #endif

        ktxTexture2* pTexture = nullptr;

        // Texture Load
        {
            #ifdef ENGINE_PROFILE
            ZoneScopedN("CreateFromNamedFile");
            #endif

            const auto result = ktxTexture2_CreateFromNamedFile
            (
                path.data(),
                KTX_TEXTURE_CREATE_NO_FLAGS,
                &pTexture
            );

            if (result != KTX_SUCCESS)
            {
                Logger::Error("Failed to load KTX2 file! [Error={}] [Path={}]", ktxErrorString(result), path);
            }

            if (pTexture->isVideo)
            {
                Logger::Error("Videos are not supported! [Path={}]", path);
            }

            if (pTexture->isCubemap)
            {
                Logger::Error("Cubemaps are not supported! [Path={}]", path);
            }
        }

        if (ktxTexture2_NeedsTranscoding(pTexture))
        {
            #ifdef ENGINE_PROFILE
            ZoneScopedN("TranscodeBasis");
            #endif

            const auto result = ktxTexture2_TranscodeBasis(pTexture, KTX_TTF_BC7_RGBA, 0);

            if (result != KTX_SUCCESS)
            {
                Logger::Error("Failed to transcode KTX2 file! [Error={}] [Path={}]", ktxErrorString(result), path);
            }
        }

        auto buffer = Vk::Buffer
        (
            allocator,
            pTexture->dataSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            VMA_MEMORY_USAGE_AUTO
        );

        std::vector<VkBufferImageCopy2> copyRegions = {};

        // Copy
        {
            #ifdef ENGINE_PROFILE
            ZoneScopedN("Copy");
            #endif

            std::memcpy(buffer.allocationInfo.pMappedData, pTexture->pData, pTexture->dataSize);

            for (u32 mipLevel = 0; mipLevel < pTexture->numLevels; ++mipLevel)
            {
                const u32 mipWidth  = std::max(pTexture->baseWidth  >> mipLevel, 1u);
                const u32 mipHeight = std::max(pTexture->baseHeight >> mipLevel, 1u);

                for (u32 arrayLayer = 0; arrayLayer < pTexture->numLayers; ++arrayLayer)
                {
                    ktx_size_t offset = 0;
                    ktxTexture2_GetImageOffset(pTexture, mipLevel, arrayLayer, 0, &offset);

                    copyRegions.emplace_back(VkBufferImageCopy2{
                        .sType             = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
                        .pNext             = nullptr,
                        .bufferOffset      = offset,
                        .bufferRowLength   = 0,
                        .bufferImageHeight = 0,
                        .imageSubresource  = {
                            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                            .mipLevel       = mipLevel,
                            .baseArrayLayer = arrayLayer,
                            .layerCount     = 1
                        },
                        .imageOffset = {0, 0, 0},
                        .imageExtent = {mipWidth, mipHeight, 1}
                    });
                }
            }
        }

        const auto image = Vk::Image
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
                .arrayLayers           = pTexture->numLayers,
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

        ktxTexture2_Destroy(pTexture);

        AppendUpload(Upload{image, buffer, copyRegions});

        deletionQueue.PushDeletor([allocator, buffer] () mutable
        {
            buffer.Destroy(allocator);
        });

        return image;
    }


    Vk::Image ImageUploader::LoadImageHDR
    (
        VmaAllocator allocator,
        Util::DeletionQueue& deletionQueue,
        const std::string_view path
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScopedN("STB HDRi Loader");
        #endif

        s32 _width  = 0;
        s32 _height = 0;

        // HDRi Environment Maps are always flipped for some reason idk why
        stbi_set_flip_vertically_on_load_thread(true);

        const f32* data = stbi_loadf
        (
            path.data(),
            &_width,
            &_height,
            nullptr,
            STBI_rgb_alpha
        );

        stbi_set_flip_vertically_on_load_thread(false);

        if (data == nullptr)
        {
            Logger::Error("Unable to load texture! [Error={}] [Path={}]\n", stbi_failure_reason(), path);
        }

        const u32 width  = _width;
        const u32 height = _height;

        const usize        texelCount = static_cast<usize>(width) * height;
        const usize        elemCount  = texelCount * STBI_rgb_alpha;
        const VkDeviceSize dataSize   = elemCount * sizeof(f16);

        auto buffer = Vk::Buffer
        (
            allocator,
            dataSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            VMA_MEMORY_USAGE_AUTO
        );

        Util::ConvertF32ToF16(data, static_cast<f16*>(buffer.allocationInfo.pMappedData), elemCount);

        stbi_image_free(std::bit_cast<void*>(data));

        const std::vector copyRegions = {VkBufferImageCopy2{
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
            .imageExtent = {width, height, 1}
        }};

        const auto image = Vk::Image
        (
            allocator,
            VkImageCreateInfo{
                .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext                 = nullptr,
                .flags                 = 0,
                .imageType             = VK_IMAGE_TYPE_2D,
                .format                = VK_FORMAT_R16G16B16A16_SFLOAT,
                .extent                = {width, height, 1},
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

        AppendUpload(Upload{image, buffer, copyRegions});

        deletionQueue.PushDeletor([allocator, buffer] () mutable
        {
            buffer.Destroy(allocator);
        });

        return image;
    }

    Vk::Image ImageUploader::LoadImageSTBI
    (
        VmaAllocator allocator,
        Util::DeletionQueue& deletionQueue,
        const std::string_view path
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScopedN("STB Standard Loader");
        #endif

        s32 _width  = 0;
        s32 _height = 0;

        const u8* data = stbi_load
        (
            path.data(),
            &_width,
            &_height,
            nullptr,
            STBI_rgb_alpha
        );

        if (data == nullptr)
        {
            Logger::Error("Unable to load texture! [Error={}] [Path={}]\n", stbi_failure_reason(), path);
        }

        const u32 width  = _width;
        const u32 height = _height;

        const usize        texelCount = static_cast<usize>(width) * height;
        const usize        elemCount  = texelCount * STBI_rgb_alpha;
        const VkDeviceSize dataSize   = elemCount * sizeof(u8);

        auto buffer = Vk::Buffer
        (
            allocator,
            dataSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            VMA_MEMORY_USAGE_AUTO
        );

        std::memcpy(buffer.allocationInfo.pMappedData, data, dataSize);

        stbi_image_free(std::bit_cast<void*>(data));

        const std::vector copyRegions = {VkBufferImageCopy2{
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
            .imageExtent = {width, height, 1}
        }};

        const auto image = Vk::Image
        (
            allocator,
            VkImageCreateInfo{
                .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext                 = nullptr,
                .flags                 = 0,
                .imageType             = VK_IMAGE_TYPE_2D,
                .format                = VK_FORMAT_R8G8B8A8_SRGB,
                .extent                = {width, height, 1},
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

        AppendUpload(Upload{image, buffer, copyRegions});

        deletionQueue.PushDeletor([allocator, buffer] () mutable
        {
            buffer.Destroy(allocator);
        });

        return image;
    }

    Vk::Image ImageUploader::LoadImageFromMemory
    (
        VmaAllocator allocator,
        Util::DeletionQueue& deletionQueue,
        VkFormat format,
        const void* data,
        u32 width,
        u32 height
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        const VkDeviceSize dataSize = (static_cast<usize>(width) * height) * static_cast<VkDeviceSize>(vkuFormatTexelSize(format));

        auto buffer = Vk::Buffer
        (
            allocator,
            dataSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            VMA_MEMORY_USAGE_AUTO
        );

        std::memcpy(buffer.allocationInfo.pMappedData, data, dataSize);

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
            .imageExtent       = {width, height, 1}
        });

        const auto image = Vk::Image
        (
            allocator,
            {
                .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext                 = nullptr,
                .flags                 = 0,
                .imageType             = VK_IMAGE_TYPE_2D,
                .format                = format,
                .extent                = {width, height, 1},
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

        AppendUpload(Upload{image, buffer, copyRegions});

        deletionQueue.PushDeletor([allocator, buffer] () mutable
        {
            buffer.Destroy(allocator);
        });

        return image;
    }

    void ImageUploader::AppendUpload(const Upload& upload)
    {
        std::lock_guard lock(m_uploadMutex);

        m_pendingUploads.emplace_back(upload);
    }

    void ImageUploader::FlushUploads(const Vk::CommandBuffer& cmdBuffer)
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        if (!HasPendingUploads())
        {
            return;
        }

        std::lock_guard lock(m_uploadMutex);

        // Undefined -> Transfer Destination
        {
            for (const auto& upload : m_pendingUploads)
            {
                m_barrierWriter.WriteImageBarrier
                (
                    upload.image,
                    Vk::ImageBarrier{
                        .srcStageMask   = VK_PIPELINE_STAGE_2_NONE,
                        .srcAccessMask  = VK_ACCESS_2_NONE,
                        .dstStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                        .dstAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                        .oldLayout      = VK_IMAGE_LAYOUT_UNDEFINED,
                        .newLayout      = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        .baseMipLevel   = 0,
                        .levelCount     = upload.image.mipLevels,
                        .baseArrayLayer = 0,
                        .layerCount     = upload.image.arrayLayers
                    }
                );
            }

            m_barrierWriter.Execute(cmdBuffer);
        }

        // Buffer to Image Copy
        {
            for (const auto& [image, buffer, copyRegions] : m_pendingUploads)
            {
                const VkCopyBufferToImageInfo2 copyInfo =
                {
                    .sType          = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
                    .pNext          = nullptr,
                    .srcBuffer      = buffer.handle,
                    .dstImage       = image.handle,
                    .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .regionCount    = static_cast<u32>(copyRegions.size()),
                    .pRegions       = copyRegions.data()
                };

                vkCmdCopyBufferToImage2(cmdBuffer.handle, &copyInfo);
            }
        }

        // Transfer Destination -> Shader Read Only
        {
            for (const auto& upload : m_pendingUploads)
            {
                m_barrierWriter.WriteImageBarrier
                (
                    upload.image,
                    Vk::ImageBarrier{
                        .srcStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                        .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                        .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                        .oldLayout      = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        .baseMipLevel   = 0,
                        .levelCount     = upload.image.mipLevels,
                        .baseArrayLayer = 0,
                        .layerCount     = upload.image.arrayLayers
                    }
                );
            }

            m_barrierWriter.Execute(cmdBuffer);
        }

        m_pendingUploads.clear();
    }

    bool ImageUploader::HasPendingUploads()
    {
        std::lock_guard lock(m_uploadMutex);

        return !m_pendingUploads.empty();
    }

    void ImageUploader::Clear()
    {
        std::lock_guard lock(m_uploadMutex);

        m_pendingUploads.clear();
        m_barrierWriter.Clear();
    }
}
