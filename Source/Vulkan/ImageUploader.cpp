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
#include <vulkan/utility/vk_format_utils.h>

#include "Util/Log.h"
#include "Util/SIMD.h"
#include "Util/Visitor.h"
#include "Util/Enum.h"
#include "Externals/STB.h"
#include "Externals/OpenEXR.h"
#include "Externals/Tracy.h"

namespace Vk
{
    Vk::Image ImageUploader::LoadImage
    (
        VmaAllocator allocator,
        Util::DeletionQueue& deletionQueue,
        const Vk::ImageUpload& upload
    )
    {
        return std::visit(Util::Visitor{
            [&] (const ImageUploadFile& file) -> Vk::Image
            {
                return LoadFromFile
                (
                    allocator,
                    deletionQueue,
                    file.path,
                    upload.type,
                    upload.flags
                );
            },
            [&] (const ImageUploadMemory& memory) -> Vk::Image
            {
                return LoadFromMemory
                (
                    allocator,
                    deletionQueue,
                    memory,
                    upload.type,
                    upload.flags
                );
            },
            [&] (const ImageUploadRawMemory& rawMemory) -> Vk::Image
            {
                return LoadRawMemory
                (
                    allocator,
                    deletionQueue,
                    rawMemory
                );
            }
        }, upload.source);
    }

    void ImageUploader::FlushUploads(const Vk::CommandBuffer& cmdBuffer)
    {
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

    Vk::Image ImageUploader::LoadFromFile
    (
        VmaAllocator allocator,
        Util::DeletionQueue& deletionQueue,
        const std::string_view path,
        ImageUploadType type,
        ImageUploadFlags flags
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneNamed(zone, true);
        zone.NameFmt("%s", std::string(Util::Files::GetName(path)).c_str());
        #endif

        switch (type)
        {
        case ImageUploadType::SDR:
            return LoadSTBIFile(allocator, deletionQueue, path, flags);

        case ImageUploadType::HDR:
            return LoadHDRFile(allocator, deletionQueue, path, flags);

        case ImageUploadType::EXR:
            return LoadEXRFile(allocator, deletionQueue, path, flags);

        case ImageUploadType::KTX2:
            return LoadKTX2File(allocator, deletionQueue, path);

        default:
            Logger::Error("{}\n", "Invalid image type!");
        }
    }

    Vk::Image ImageUploader::LoadFromMemory
    (
       VmaAllocator allocator,
       Util::DeletionQueue& deletionQueue,
       const Vk::ImageUploadMemory& memory,
       ImageUploadType type,
       ImageUploadFlags flags
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        switch (type)
        {
            case ImageUploadType::SDR:
                return LoadSTBIMemory(allocator, deletionQueue, memory, flags);

            case ImageUploadType::HDR:
                return LoadHDRMemory(allocator, deletionQueue, memory, flags);

            case ImageUploadType::KTX2:
                return LoadKTX2Memory(allocator, deletionQueue, memory);

            default:
                Logger::Error("{}\n", "Invalid image type!");
        }
    }

    Vk::Image ImageUploader::LoadSTBIFile
    (
        VmaAllocator allocator,
        Util::DeletionQueue& deletionQueue,
        const std::string_view path,
        ImageUploadFlags flags
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        // Flags
        const bool toFlip = (flags & ImageUploadFlags::Flipped) == ImageUploadFlags::Flipped;

        s32 _width  = 0;
        s32 _height = 0;

        stbi_set_flip_vertically_on_load_thread(toFlip);

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
            Logger::Error("Unable to load texture! [Error={}] [Path={}]\n", stbi_failure_reason(), path.data());
        }

        const u32 width  = _width;
        const u32 height = _height;

        return LoadSTBIInternal
        (
            allocator,
            deletionQueue,
            data,
            width,
            height
        );
    }

    Vk::Image ImageUploader::LoadSTBIMemory
    (
        VmaAllocator allocator,
        Util::DeletionQueue& deletionQueue,
        const Vk::ImageUploadMemory& memory,
        ImageUploadFlags flags
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        // Flags
        const bool toFlip = (flags & ImageUploadFlags::Flipped) == ImageUploadFlags::Flipped;

        s32 _width  = 0;
        s32 _height = 0;

        stbi_set_flip_vertically_on_load_thread(toFlip);

        const u8* data = stbi_load_from_memory
        (
            memory.data.data(),
            static_cast<s32>(memory.data.size()),
            &_width,
            &_height,
            nullptr,
            STBI_rgb_alpha
        );

        if (data == nullptr)
        {
            Logger::Error("Unable to load texture! [Error={}]\n", stbi_failure_reason());
        }

        const u32 width  = _width;
        const u32 height = _height;

        return LoadSTBIInternal
        (
            allocator,
            deletionQueue,
            data,
            width,
            height
        );
    }

    Vk::Image ImageUploader::LoadSTBIInternal
    (
        VmaAllocator allocator,
        Util::DeletionQueue& deletionQueue,
        const u8* data,
        u32 width,
        u32 height
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

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

    Vk::Image ImageUploader::LoadHDRFile
    (
        VmaAllocator allocator,
        Util::DeletionQueue& deletionQueue,
        const std::string_view path,
        ImageUploadFlags flags
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        // Flags
        const bool toFlip = (flags & ImageUploadFlags::Flipped) == ImageUploadFlags::Flipped;

        s32 _width  = 0;
        s32 _height = 0;

        stbi_set_flip_vertically_on_load_thread(toFlip);

        const f32* data = stbi_loadf
        (
            path.data(),
            &_width,
            &_height,
            nullptr,
            STBI_rgb_alpha
        );

        if (data == nullptr)
        {
            Logger::Error("Unable to load texture! [Error={}] [Path={}]\n", stbi_failure_reason(), path.data());
        }

        const u32 width  = _width;
        const u32 height = _height;

        return LoadHDRInternal
        (
            allocator,
            deletionQueue,
            data,
            width,
            height,
            flags
        );
    }

    Vk::Image ImageUploader::LoadHDRMemory
    (
        VmaAllocator allocator,
        Util::DeletionQueue& deletionQueue,
        const Vk::ImageUploadMemory& memory,
        ImageUploadFlags flags
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        // Flags
        const bool toFlip = (flags & ImageUploadFlags::Flipped) == ImageUploadFlags::Flipped;

        s32 _width  = 0;
        s32 _height = 0;

        stbi_set_flip_vertically_on_load_thread(toFlip);

        const f32* data = stbi_loadf_from_memory
        (
            memory.data.data(),
            static_cast<s32>(memory.data.size()),
            &_width,
            &_height,
            nullptr,
            STBI_rgb_alpha
        );

        if (data == nullptr)
        {
            Logger::Error("Unable to load texture! [Error={}]\n", stbi_failure_reason());
        }

        const u32 width  = _width;
        const u32 height = _height;

        return LoadHDRInternal
        (
            allocator,
            deletionQueue,
            data,
            width,
            height,
            flags
        );
    }

    Vk::Image ImageUploader::LoadHDRInternal
    (
        VmaAllocator allocator,
        Util::DeletionQueue& deletionQueue,
        const f32* data,
        u32 width,
        u32 height,
        ImageUploadFlags flags
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        // Flags
        const bool toF16 = (flags & ImageUploadFlags::F16) == ImageUploadFlags::F16;

        const usize        texelCount = static_cast<usize>(width) * height;
        const usize        elemCount  = texelCount * STBI_rgb_alpha;
        const VkDeviceSize elemSize   = toF16 ? sizeof(f16) : sizeof(f32);
        const VkDeviceSize dataSize   = elemCount * elemSize;

        auto buffer = Vk::Buffer
        (
            allocator,
            dataSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            VMA_MEMORY_USAGE_AUTO
        );

        if (toF16)
        {
            Util::ConvertF32ToF16(data, static_cast<f16*>(buffer.allocationInfo.pMappedData), elemCount);
        }
        else
        {
            std::memcpy(buffer.allocationInfo.pMappedData, data, dataSize);
        }

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
                .format                = toF16 ? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_R32G32B32A32_SFLOAT,
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

    Vk::Image ImageUploader::LoadEXRFile
    (
        VmaAllocator allocator,
        Util::DeletionQueue& deletionQueue,
        const std::string_view path,
        ImageUploadFlags flags
    )
    {
        try
        {
            #ifdef ENGINE_PROFILE
            ZoneScoped;
            #endif

            Imf::RgbaInputFile file(path.data());

            const Imath::Box2i dataWindow = file.dataWindow();
            const s32          width      = dataWindow.max.x - dataWindow.min.x + 1;
            const s32          height     = dataWindow.max.y - dataWindow.min.y + 1;

            Imf::Array2D<Imf::Rgba> pixels(width, height);

            file.setFrameBuffer(&pixels[0][0], 1, width);
            file.readPixels(dataWindow.min.y, dataWindow.max.y);

            // Flags
            const bool toF16  = (flags & ImageUploadFlags::F16)     == ImageUploadFlags::F16;

            const usize        texelCount = static_cast<usize>(width) * height;
            const usize        elemCount  = 4 * texelCount;
            const VkDeviceSize elemSize   = toF16 ? sizeof(f16) : sizeof(f32);
            const VkDeviceSize dataSize   = elemCount * elemSize;

            auto buffer = Vk::Buffer
            (
                allocator,
                dataSize,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            if (toF16)
            {
                std::memcpy(buffer.allocationInfo.pMappedData, &pixels[0][0], dataSize);
            }
            else
            {
                Util::ConvertF16ToF32(reinterpret_cast<const f16*>(&pixels[0][0]), static_cast<f32*>(buffer.allocationInfo.pMappedData), elemCount);
            }

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
                .imageExtent = {static_cast<u32>(width), static_cast<u32>(height), 1}
            }};

            const auto image = Vk::Image
            (
                allocator,
                VkImageCreateInfo{
                    .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                    .pNext                 = nullptr,
                    .flags                 = 0,
                    .imageType             = VK_IMAGE_TYPE_2D,
                    .format                = toF16 ? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_R32G32B32A32_SFLOAT,
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

            AppendUpload(Upload{image, buffer, copyRegions});

            deletionQueue.PushDeletor([allocator, buffer] () mutable
            {
                buffer.Destroy(allocator);
            });

            return image;
        }
        catch (const std::exception& e)
        {
            Logger::Error("Failed to read EXR file! [Error={}] [Path={}]\n", e.what(), path);
        }
    }

    Vk::Image ImageUploader::LoadKTX2File
    (
        VmaAllocator allocator,
        Util::DeletionQueue& deletionQueue,
        const std::string_view path
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        ktxTexture2* pTexture = nullptr;

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

        return LoadKTX2Internal(allocator, deletionQueue, pTexture);
    }

    Vk::Image ImageUploader::LoadKTX2Memory
    (
        VmaAllocator allocator,
        Util::DeletionQueue& deletionQueue,
        const Vk::ImageUploadMemory& memory
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        ktxTexture2* pTexture = nullptr;

        const auto result = ktxTexture2_CreateFromMemory
        (
            memory.data.data(),
            memory.data.size(),
            KTX_TEXTURE_CREATE_NO_FLAGS,
            &pTexture
        );

        if (result != KTX_SUCCESS)
        {
            Logger::Error("Failed to load KTX2 file! [Error={}] [Name={}]", ktxErrorString(result), memory.name);
        }

        return LoadKTX2Internal(allocator, deletionQueue, pTexture);
    }

    Vk::Image ImageUploader::LoadKTX2Internal
    (
        VmaAllocator allocator,
        Util::DeletionQueue& deletionQueue,
        ktxTexture2* pTexture
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        if (pTexture->isVideo)
        {
            Logger::Error("{}\n", "Videos are not supported!");
        }

        if (pTexture->isCubemap)
        {
            Logger::Error("{}\n", "Cubemaps are not supported!");
        }

        if (ktxTexture2_NeedsTranscoding(pTexture))
        {
            #ifdef ENGINE_PROFILE
            ZoneScopedN("TranscodeBasis");
            #endif

            const auto result = ktxTexture2_TranscodeBasis(pTexture, KTX_TTF_BC7_RGBA, 0);

            if (result != KTX_SUCCESS)
            {
                Logger::Error("Failed to transcode to BC7! [Error={}]", ktxErrorString(result));
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

    Vk::Image ImageUploader::LoadRawMemory
    (
        VmaAllocator allocator,
        Util::DeletionQueue& deletionQueue,
        const ImageUploadRawMemory& rawMemory
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        if (rawMemory.data.empty() || rawMemory.width == 0 || rawMemory.height == 0 || rawMemory.format == VK_FORMAT_UNDEFINED)
        {
            Logger::Error("{}\n", "Invalid parameters!");
        }

        const auto dataSize = static_cast<VkDeviceSize>(static_cast<f64>(static_cast<usize>(rawMemory.width) * rawMemory.height) * vkuFormatTexelSize(rawMemory.format));

        auto buffer = Vk::Buffer
        (
            allocator,
            dataSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            VMA_MEMORY_USAGE_AUTO
        );

        std::memcpy(buffer.allocationInfo.pMappedData, rawMemory.data.data(), dataSize);

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
            .imageExtent       = {rawMemory.width, rawMemory.height, 1}
        });

        const auto image = Vk::Image
        (
            allocator,
            {
                .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext                 = nullptr,
                .flags                 = 0,
                .imageType             = VK_IMAGE_TYPE_2D,
                .format                = rawMemory.format,
                .extent                = {rawMemory.width, rawMemory.height, 1},
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

    void ImageUploader::AppendUpload(Upload&& upload)
    {
        std::lock_guard lock(m_uploadMutex);

        m_pendingUploads.emplace_back(upload);
    }
}
