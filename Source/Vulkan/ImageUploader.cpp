/*
 * Copyright (c) 2023 - 2026 Rachit
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
#include <volk/volk.h>

#include "Util.h"
#include "Engine/Cache.h"
#include "Util/Log.h"
#include "Util/SIMD.h"
#include "Util/Visitor.h"
#include "Util/Enum.h"
#include "Util/Files.h"
#include "Externals/STB.h"
#include "Externals/OpenEXR.h"
#include "Vulkan/DebugUtils.h"

#ifdef ENGINE_PROFILE
#include "Externals/Tracy.h"
#endif

namespace Vk
{
    constexpr VkFormat SDR_FORMAT = VK_FORMAT_R8G8B8A8_SRGB;

    Vk::ImageUploadType FileToImageUploadType(const std::string_view file)
    {
        const auto extension = Files::GetExtension(file);

        auto type = Vk::ImageUploadType::SDR;

        if (extension == ".hdr")
        {
            type = Vk::ImageUploadType::HDR;
        }
        else if (extension == ".exr")
        {
            type = Vk::ImageUploadType::EXR;
        }
        else if (extension == ".ktx2")
        {
            type = Vk::ImageUploadType::KTX2;
        }

        return type;
    }

    usize GetImageFileUploadHash(Vk::ImageUploadFlags flags, const std::string_view path)
    {
        usize hash = 0;

        hash = Util::HashCombine(hash, static_cast<u8>(flags));
        hash = Util::HashCombine(hash, Files::GetLastWriteTime(path));

        return hash;
    }

    std::string GetImageUploadCacheFileName(const std::string_view path)
    {
        return fmt::format
        (
            "{}_{}.cache",
            Files::GetName(path),
            std::hash<std::string_view>{}(path)
        );
    }

    Vk::UploadedImage ImageUploader::LoadImage
    (
        VkDevice device,
        VmaAllocator allocator,
        Vk::StagingPool& stagingPool,
        tf::Executor& executor,
        Engine::DeletionQueue& deletionQueue,
        const Vk::ImageUpload& upload
    )
    {
        return std::visit(Util::Visitor{
            [&] (const ImageUploadFile& file) -> Vk::UploadedImage
            {
                return LoadFromFile
                (
                    device,
                    allocator,
                    stagingPool,
                    executor,
                    deletionQueue,
                    file.path,
                    upload.type,
                    upload.flags
                );
            },
            [&] (const ImageUploadMemory& memory) -> Vk::UploadedImage
            {
                return LoadFromMemory
                (
                    device,
                    allocator,
                    stagingPool,
                    deletionQueue,
                    memory,
                    upload.type,
                    upload.flags
                );
            },
            [&] (const ImageUploadRawMemory& rawMemory) -> Vk::UploadedImage
            {
                return LoadRawMemory
                (
                    device,
                    allocator,
                    stagingPool,
                    deletionQueue,
                    rawMemory,
                    upload.flags
                );
            },
            [&] (const ImageUploadCache& cache) -> Vk::UploadedImage
            {
                return LoadCache
                (
                    device,
                    allocator,
                    stagingPool,
                    deletionQueue,
                    cache
                );
            }
        }, upload.source);
    }

    void ImageUploader::UpdateImage
    (
        VkDevice device,
        VmaAllocator allocator,
        Vk::StagingPool& stagingPool,
        Engine::DeletionQueue& deletionQueue,
        const Vk::Image& image,
        const Vk::ImageUpdateRawMemory& updateRawMemory
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        const auto pixelCount = static_cast<usize>(updateRawMemory.update.extent.width) * static_cast<usize>(updateRawMemory.update.extent.height);
        const auto texelSize  = Vk::GetTexelSize(image.format);
        const auto updateSize = static_cast<VkDeviceSize>(static_cast<f64>(pixelCount) * texelSize);

        const auto stagingMemoryBlock = stagingPool.Allocate
        (
            device,
            allocator,
            updateSize,
            vkuFormatTexelBlockSize(image.format)
        );

        std::memcpy(stagingMemoryBlock.hostAddress, updateRawMemory.data.data(), updateSize);

        std::vector<VkBufferImageCopy2> copyRegions = {};

        copyRegions.emplace_back(VkBufferImageCopy2{
            .sType             = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
            .pNext             = nullptr,
            .bufferOffset      = stagingMemoryBlock.memoryBlock.offset,
            .bufferRowLength   = 0,
            .bufferImageHeight = 0,
            .imageSubresource  = {
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel       = 0,
                .baseArrayLayer = 0,
                .layerCount     = 1
            },
            .imageOffset       = {.x     = updateRawMemory.update.offset.x,     .y      = updateRawMemory.update.offset.y,      .z     = 0},
            .imageExtent       = {.width = updateRawMemory.update.extent.width, .height = updateRawMemory.update.extent.height, .depth = 1}
        });

        AppendUpload(Upload{
            .image           = image,
            .buffer          = stagingMemoryBlock.buffer,
            .copyRegions     = copyRegions,
            .srcStageMask    = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .srcAccessMask   = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            .oldLayout       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .generateMipmaps = false
        });

        deletionQueue.Push([&stagingPool, stagingMemoryBlock] () mutable
        {
            stagingPool.Free(stagingMemoryBlock);
        });
    }

    void ImageUploader::FlushUploads(const Vk::CommandBuffer& cmdBuffer, Scratch::Allocator& scratchAllocator)
    {
        const std::scoped_lock lock{m_mutex};

        if (m_pendingUploads.empty())
        {
            return;
        }

        Vk::BeginLabel(cmdBuffer, "ImageUploader::FlushUploads", {0.6117f, 0.8196f, 0.0313f, 1.0f});

        auto barrierWriter = Vk::ScratchBarrierWriter(scratchAllocator);

        // ? -> Transfer Destination
        {
            for (const auto& upload : m_pendingUploads)
            {
                barrierWriter.WriteImageBarrier
                (
                    upload.image,
                    Vk::ImageBarrier{
                        .srcStageMask   = upload.srcStageMask,
                        .srcAccessMask  = upload.srcAccessMask,
                        .dstStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                        .dstAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                        .oldLayout      = upload.oldLayout,
                        .newLayout      = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                        .baseMipLevel   = 0,
                        .levelCount     = upload.image.mipLevels,
                        .baseArrayLayer = 0,
                        .layerCount     = upload.image.arrayLayers
                    }
                );
            }

            barrierWriter.Execute(cmdBuffer);
        }

        // Buffer to Image Copy
        {
            for (const auto& upload : m_pendingUploads)
            {
                const VkCopyBufferToImageInfo2 copyInfo =
                {
                    .sType          = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
                    .pNext          = nullptr,
                    .srcBuffer      = upload.buffer,
                    .dstImage       = upload.image.handle,
                    .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .regionCount    = static_cast<u32>(upload.copyRegions.size()),
                    .pRegions       = upload.copyRegions.data()
                };

                vkCmdCopyBufferToImage2(cmdBuffer.handle, &copyInfo);
            }
        }

        // Mipmap Generation Barriers
        {
            for (const auto& upload : m_pendingUploads)
            {
                if (!upload.generateMipmaps || upload.image.mipLevels <= 1)
                {
                    continue;
                }

                barrierWriter.WriteImageBarrier
                (
                    upload.image,
                    Vk::ImageBarrier{
                        .srcStageMask    = VK_PIPELINE_STAGE_2_COPY_BIT,
                        .srcAccessMask   = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                        .dstStageMask    = VK_PIPELINE_STAGE_2_BLIT_BIT,
                        .dstAccessMask   = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                        .oldLayout       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        .newLayout       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        .srcQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                        .baseMipLevel    = 0,
                        .levelCount      = upload.image.mipLevels,
                        .baseArrayLayer  = 0,
                        .layerCount      = upload.image.arrayLayers
                    }
                );
            }

            barrierWriter.Execute(cmdBuffer);
        }

        // Mipmap Generation
        {
            for (const auto& upload : m_pendingUploads)
            {
                if (!upload.generateMipmaps || upload.image.mipLevels <= 1)
                {
                    continue;
                }

                upload.image.GenerateMipmaps(cmdBuffer, scratchAllocator);
            }
        }

        // Transfer Destination -> Shader Read Only
        {
            for (const auto& upload : m_pendingUploads)
            {
                if (upload.generateMipmaps && upload.image.mipLevels > 1)
                {
                    continue;
                }

                barrierWriter.WriteImageBarrier
                (
                    upload.image,
                    Vk::ImageBarrier{
                        .srcStageMask    = VK_PIPELINE_STAGE_2_COPY_BIT,
                        .srcAccessMask   = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                        .dstStageMask    = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        .dstAccessMask   = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                        .oldLayout       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        .newLayout       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        .srcQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                        .baseMipLevel    = 0,
                        .levelCount      = upload.image.mipLevels,
                        .baseArrayLayer  = 0,
                        .layerCount      = upload.image.arrayLayers
                    }
                );
            }

            barrierWriter.Execute(cmdBuffer);
        }

        m_pendingUploads.clear();

        Vk::EndLabel(cmdBuffer);
    }

    bool ImageUploader::HasPendingUploads()
    {
        const std::scoped_lock lock{m_mutex};

        return !m_pendingUploads.empty();
    }

    void ImageUploader::Clear()
    {
        const std::scoped_lock lock{m_mutex};

        m_pendingUploads.clear();
    }

    Vk::UploadedImage ImageUploader::LoadFromFile
    (
        VkDevice device,
        VmaAllocator allocator,
        Vk::StagingPool& stagingPool,
        tf::Executor& executor,
        Engine::DeletionQueue& deletionQueue,
        const std::string_view path,
        Vk::ImageUploadType type,
        Vk::ImageUploadFlags flags
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneNamed(zone, true);
        zone.NameFmt("%s", std::string(Files::GetName(path)).c_str());
        #endif

        switch (type)
        {
        case ImageUploadType::SDR:
            return LoadSTBIFile
            (
                device,
                allocator,
                stagingPool,
                executor,
                deletionQueue,
                path,
                flags
            );

        case ImageUploadType::HDR:
            return LoadHDRFile
            (
                device,
                allocator,
                stagingPool,
                executor,
                deletionQueue,
                path,
                flags
            );

        case ImageUploadType::EXR:
            return LoadEXRFile
            (
                device,
                allocator,
                stagingPool,
                executor,
                deletionQueue,
                path,
                flags
            );

        case ImageUploadType::KTX2:
            return LoadKTX2File
            (
                device,
                allocator,
                stagingPool,
                executor,
                deletionQueue,
                path
            );

        default:
            Logger::Error("{}\n", "Invalid image type!");
        }
    }

    Vk::UploadedImage ImageUploader::LoadFromMemory
    (
        VkDevice device,
       VmaAllocator allocator,
       Vk::StagingPool& stagingPool,
       Engine::DeletionQueue& deletionQueue,
       const Vk::ImageUploadMemory& memory,
       Vk::ImageUploadType type,
       Vk::ImageUploadFlags flags
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        switch (type)
        {
            case ImageUploadType::SDR:
                return LoadSTBIMemory
                (
                    device,
                    allocator,
                    stagingPool,
                    deletionQueue,
                    memory,
                    flags
                );

            case ImageUploadType::HDR:
                return LoadHDRMemory
                (
                    device,
                    allocator,
                    stagingPool,
                    deletionQueue,
                    memory,
                    flags
                );

            case ImageUploadType::KTX2:
                return LoadKTX2Memory
                (
                    device,
                    allocator,
                    stagingPool,
                    deletionQueue,
                    memory
                );

            default:
                Logger::Error("{}\n", "Invalid image type!");
        }
    }

    Vk::UploadedImage ImageUploader::LoadSTBIFile
    (
        VkDevice device,
        VmaAllocator allocator,
        Vk::StagingPool& stagingPool,
        tf::Executor& executor,
        Engine::DeletionQueue& deletionQueue,
        const std::string_view path,
        Vk::ImageUploadFlags flags
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        const auto cacheFile = GetImageUploadCacheFileName(path);

        const usize hash = GetImageFileUploadHash(flags, path);

        const Cache::Query query =
        {
            .cachedFile = cacheFile,
            .assetType  = Cache::AssetType::Texture,
            .hash       = hash
        };

        if (Cache::IsInCache(query))
        {
            const auto cache = Vk::ImageUploadCache
            {
                .name       = Files::GetNameWithoutExtension(path),
                .cachedPath = cacheFile
            };

            return LoadCache
            (
                device,
                allocator,
                stagingPool,
                deletionQueue,
                cache
            );
        }

        const bool toFlip = (flags & ImageUploadFlags::Flipped) == ImageUploadFlags::Flipped;

        s32 _width  = 0;
        s32 _height = 0;

        stbi_set_flip_vertically_on_load_thread(toFlip);

        u8* data = stbi_load
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

        const auto uploadedImage = LoadSTBIInternal
        (
            device,
            allocator,
            stagingPool,
            deletionQueue,
            data,
            width,
            height,
            flags
        );

        executor.silent_async([width, height, data, flags, cacheFile, hash] ()
        {
            const usize        texelCount = static_cast<usize>(width) * height;
            const usize        elemCount  = texelCount * STBI_rgb_alpha;
            const VkDeviceSize dataSize   = elemCount * sizeof(u8);

            auto imageData = std::vector<u8>(dataSize);

            std::memcpy(imageData.data(), data, dataSize);

            stbi_image_free(data);

            const bool generateMipmaps = (flags & ImageUploadFlags::Mipmaps) == ImageUploadFlags::Mipmaps;

            constexpr std::array<VkDeviceSize, 1> TEXTURE_OFFSET_TABLE = {0};

            const auto textureOffsetTable = Cache::GenerateTextureOffsetTable(TEXTURE_OFFSET_TABLE);

            const u32 mipLevels = static_cast<u32>(std::floor(std::log2(std::max(width, height))) + 1);

            Cache::InsertIntoCache(Cache::Entry
            {
                .cacheFile          = cacheFile,
                .assetType          = Cache::AssetType::Texture,
                .compressionType    = Cache::CompressionType::LZ4,
                .assetHeader        = Cache::TextureHeader{
                    .width           = width,
                    .height          = height,
                    .mipLevels       = generateMipmaps ? mipLevels : 1,
                    .arrayLayers     = 1,
                    .faceCount       = 1,
                    .format          = SDR_FORMAT,
                    .generateMipmaps = generateMipmaps
                },
                .hash                 = hash,
                .additionalHeaderData = textureOffsetTable,
                .data                 = imageData,
            });
        });

        return uploadedImage;
    }

    Vk::UploadedImage ImageUploader::LoadSTBIMemory
    (
        VkDevice device,
        VmaAllocator allocator,
        Vk::StagingPool& stagingPool,
        Engine::DeletionQueue& deletionQueue,
        const Vk::ImageUploadMemory& memory,
        Vk::ImageUploadFlags flags
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        const bool toFlip = (flags & ImageUploadFlags::Flipped) == ImageUploadFlags::Flipped;

        s32 _width  = 0;
        s32 _height = 0;

        stbi_set_flip_vertically_on_load_thread(toFlip);

        u8* data = stbi_load_from_memory
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

        const auto uploadedImage = LoadSTBIInternal
        (
            device,
            allocator,
            stagingPool,
            deletionQueue,
            data,
            width,
            height,
            flags
        );

        stbi_image_free(data);

        return uploadedImage;
    }

    Vk::UploadedImage ImageUploader::LoadSTBIInternal
    (
        VkDevice device,
        VmaAllocator allocator,
        Vk::StagingPool& stagingPool,
        Engine::DeletionQueue& deletionQueue,
        const u8* data,
        u32 width,
        u32 height,
        Vk::ImageUploadFlags flags
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        const bool generateMipmaps = (flags & ImageUploadFlags::Mipmaps) == ImageUploadFlags::Mipmaps;

        const usize        texelCount = static_cast<usize>(width) * height;
        const usize        elemCount  = texelCount * STBI_rgb_alpha;
        const VkDeviceSize dataSize   = elemCount * sizeof(u8);

        const auto stagingMemoryBlock = stagingPool.Allocate
        (
            device,
            allocator,
            dataSize,
            vkuFormatTexelBlockSize(SDR_FORMAT)
        );

        std::memcpy(stagingMemoryBlock.hostAddress, data, dataSize);

        const std::vector copyRegions = {VkBufferImageCopy2{
            .sType             = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
            .pNext             = nullptr,
            .bufferOffset      = stagingMemoryBlock.memoryBlock.offset,
            .bufferRowLength   = 0,
            .bufferImageHeight = 0,
            .imageSubresource  = {
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel       = 0,
                .baseArrayLayer = 0,
                .layerCount     = 1
            },
            .imageOffset = {.x     = 0,     .y      = 0,      .z     = 0},
            .imageExtent = {.width = width, .height = height, .depth = 1}
        }};

        VkImageCreateInfo createInfo =
        {
            .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .imageType             = VK_IMAGE_TYPE_2D,
            .format                = SDR_FORMAT,
            .extent                = {.width = width, .height = height, .depth = 1},
            .mipLevels             = 1,
            .arrayLayers           = 1,
            .samples               = VK_SAMPLE_COUNT_1_BIT,
            .tiling                = VK_IMAGE_TILING_OPTIMAL,
            .usage                 = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices   = nullptr,
            .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED
        };

        if (generateMipmaps)
        {
            createInfo.mipLevels = static_cast<u32>(std::floor(std::log2(std::max(width, height))) + 1);
            createInfo.usage    |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        }

        const auto image = Vk::Image(allocator, createInfo, VK_IMAGE_ASPECT_COLOR_BIT);

        const auto imageView = Vk::ImageView
        (
            device,
            image,
            VK_IMAGE_VIEW_TYPE_2D,
            VkImageSubresourceRange{
                .aspectMask     = image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = image.arrayLayers
            }
        );

        AppendUpload(Upload{
            .image           = image,
            .buffer          = stagingMemoryBlock.buffer,
            .copyRegions     = copyRegions,
            .srcStageMask    = VK_PIPELINE_STAGE_2_NONE,
            .srcAccessMask   = VK_ACCESS_2_NONE,
            .oldLayout       = VK_IMAGE_LAYOUT_UNDEFINED,
            .generateMipmaps = generateMipmaps
        });

        deletionQueue.Push([&stagingPool, stagingMemoryBlock] () mutable
        {
            stagingPool.Free(stagingMemoryBlock);
        });

        return Vk::UploadedImage
        {
            .image     = image,
            .imageView = imageView
        };
    }

    Vk::UploadedImage ImageUploader::LoadHDRFile
    (
        VkDevice device,
        VmaAllocator allocator,
        Vk::StagingPool& stagingPool,
        tf::Executor& executor,
        Engine::DeletionQueue& deletionQueue,
        const std::string_view path,
        Vk::ImageUploadFlags flags
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        const auto cacheFile = GetImageUploadCacheFileName(path);

        const usize hash = GetImageFileUploadHash(flags, path);

        const Cache::Query query =
        {
            .cachedFile = cacheFile,
            .assetType  = Cache::AssetType::Texture,
            .hash       = hash
        };

        if (Cache::IsInCache(query))
        {
            const auto cache = Vk::ImageUploadCache
            {
                .name       = Files::GetNameWithoutExtension(path),
                .cachedPath = cacheFile
            };

            return LoadCache
            (
                device,
                allocator,
                stagingPool,
                deletionQueue,
                cache
            );
        }

        // Flags
        const bool toFlip = (flags & ImageUploadFlags::Flipped) == ImageUploadFlags::Flipped;

        s32 _width  = 0;
        s32 _height = 0;

        stbi_set_flip_vertically_on_load_thread(toFlip);

        f32* data = stbi_loadf
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

        const auto uploadedImage = LoadHDRInternal
        (
            device,
            allocator,
            stagingPool,
            deletionQueue,
            data,
            width,
            height,
            flags
        );

        executor.silent_async([width, height, data, flags, cacheFile, hash] ()
        {
            const bool toF16           = (flags & ImageUploadFlags::F16    ) == ImageUploadFlags::F16;
            const bool generateMipmaps = (flags & ImageUploadFlags::Mipmaps) == ImageUploadFlags::Mipmaps;

            const VkFormat format = toF16 ? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_R32G32B32A32_SFLOAT;

            const usize        texelCount = static_cast<usize>(width) * height;
            const usize        elemCount  = texelCount * STBI_rgb_alpha;
            const VkDeviceSize elemSize   = toF16 ? sizeof(f16) : sizeof(f32);
            const VkDeviceSize dataSize   = elemCount * elemSize;

            auto imageData = std::vector<u8>(dataSize);

            if (toF16)
            {
                SIMD::ConvertF32ToF16(data, reinterpret_cast<f16*>(imageData.data()), elemCount);
            }
            else
            {
                std::memcpy(imageData.data(), data, dataSize);
            }

            stbi_image_free(data);

            constexpr std::array<VkDeviceSize, 1> TEXTURE_OFFSET_TABLE = {0};

            const auto textureOffsetTable = Cache::GenerateTextureOffsetTable(TEXTURE_OFFSET_TABLE);

            const u32 mipLevels = static_cast<u32>(std::floor(std::log2(std::max(width, height))) + 1);

            Cache::InsertIntoCache(Cache::Entry
            {
                .cacheFile          = cacheFile,
                .assetType          = Cache::AssetType::Texture,
                .compressionType    = Cache::CompressionType::LZ4,
                .assetHeader        = Cache::TextureHeader{
                    .width           = width,
                    .height          = height,
                    .mipLevels       = generateMipmaps ? mipLevels : 1,
                    .arrayLayers     = 1,
                    .faceCount       = 1,
                    .format          = format,
                    .generateMipmaps = generateMipmaps
                },
                .hash                 = hash,
                .additionalHeaderData = textureOffsetTable,
                .data                 = imageData,
            });
        });

        return uploadedImage;
    }

    Vk::UploadedImage ImageUploader::LoadHDRMemory
    (
        VkDevice device,
        VmaAllocator allocator,
        Vk::StagingPool& stagingPool,
        Engine::DeletionQueue& deletionQueue,
        const Vk::ImageUploadMemory& memory,
        Vk::ImageUploadFlags flags
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

        f32* data = stbi_loadf_from_memory
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

        const auto uploadedImage = LoadHDRInternal
        (
            device,
            allocator,
            stagingPool,
            deletionQueue,
            data,
            width,
            height,
            flags
        );

        stbi_image_free(data);

        return uploadedImage;
    }

    Vk::UploadedImage ImageUploader::LoadHDRInternal
    (
        VkDevice device,
        VmaAllocator allocator,
        Vk::StagingPool& stagingPool,
        Engine::DeletionQueue& deletionQueue,
        const f32* data,
        u32 width,
        u32 height,
        Vk::ImageUploadFlags flags
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        const bool toF16           = (flags & ImageUploadFlags::F16    ) == ImageUploadFlags::F16;
        const bool generateMipmaps = (flags & ImageUploadFlags::Mipmaps) == ImageUploadFlags::Mipmaps;

        const VkFormat format = toF16 ? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_R32G32B32A32_SFLOAT;

        const usize        texelCount = static_cast<usize>(width) * height;
        const usize        elemCount  = texelCount * STBI_rgb_alpha;
        const VkDeviceSize elemSize   = toF16 ? sizeof(f16) : sizeof(f32);
        const VkDeviceSize dataSize   = elemCount * elemSize;

        const auto stagingMemoryBlock = stagingPool.Allocate
        (
            device,
            allocator,
            dataSize,
            vkuFormatTexelBlockSize(format)
        );

        if (toF16)
        {
            SIMD::ConvertF32ToF16(data, static_cast<f16*>(stagingMemoryBlock.hostAddress), elemCount);
        }
        else
        {
            std::memcpy(stagingMemoryBlock.hostAddress, data, dataSize);
        }

        const std::vector copyRegions = {VkBufferImageCopy2{
            .sType             = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
            .pNext             = nullptr,
            .bufferOffset      = stagingMemoryBlock.memoryBlock.offset,
            .bufferRowLength   = 0,
            .bufferImageHeight = 0,
            .imageSubresource  = {
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel       = 0,
                .baseArrayLayer = 0,
                .layerCount     = 1
            },
            .imageOffset = {.x     = 0,     .y      = 0,      .z     = 0},
            .imageExtent = {.width = width, .height = height, .depth = 1}
        }};

        VkImageCreateInfo createInfo =
        {
            .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .imageType             = VK_IMAGE_TYPE_2D,
            .format                = format,
            .extent                = {.width = width, .height = height, .depth = 1},
            .mipLevels             = 1,
            .arrayLayers           = 1,
            .samples               = VK_SAMPLE_COUNT_1_BIT,
            .tiling                = VK_IMAGE_TILING_OPTIMAL,
            .usage                 = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices   = nullptr,
            .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED
        };

        if (generateMipmaps)
        {
            createInfo.mipLevels = static_cast<u32>(std::floor(std::log2(std::max(width, height))) + 1);
            createInfo.usage    |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        }

        const auto image = Vk::Image(allocator, createInfo, VK_IMAGE_ASPECT_COLOR_BIT);

        const auto imageView = Vk::ImageView
        (
            device,
            image,
            VK_IMAGE_VIEW_TYPE_2D,
            VkImageSubresourceRange{
                .aspectMask     = image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = image.arrayLayers
            }
        );

        AppendUpload(Upload{
            .image           = image,
            .buffer          = stagingMemoryBlock.buffer,
            .copyRegions     = copyRegions,
            .srcStageMask    = VK_PIPELINE_STAGE_2_NONE,
            .srcAccessMask   = VK_ACCESS_2_NONE,
            .oldLayout       = VK_IMAGE_LAYOUT_UNDEFINED,
            .generateMipmaps = generateMipmaps
        });

        deletionQueue.Push([&stagingPool, stagingMemoryBlock] () mutable
        {
            stagingPool.Free(stagingMemoryBlock);
        });

        return Vk::UploadedImage
        {
            .image     = image,
            .imageView = imageView
        };
    }

    Vk::UploadedImage ImageUploader::LoadEXRFile
    (
        VkDevice device,
        VmaAllocator allocator,
        Vk::StagingPool& stagingPool,
        tf::Executor& executor,
        Engine::DeletionQueue& deletionQueue,
        const std::string_view path,
        Vk::ImageUploadFlags flags
    )
    {
        try
        {
            #ifdef ENGINE_PROFILE
            ZoneScoped;
            #endif

            const auto cacheFile = GetImageUploadCacheFileName(path);

            const usize hash = GetImageFileUploadHash(flags, path);

            const Cache::Query query =
            {
                .cachedFile = cacheFile,
                .assetType  = Cache::AssetType::Texture,
                .hash       = hash
            };

            if (Cache::IsInCache(query))
            {
                const auto cache = Vk::ImageUploadCache
                {
                    .name       = Files::GetNameWithoutExtension(path),
                    .cachedPath = cacheFile
                };

                return LoadCache
                (
                    device,
                    allocator,
                    stagingPool,
                    deletionQueue,
                    cache
                );
            }

            Imf::RgbaInputFile file(path.data(), 1);

            const Imath::Box2i dataWindow = file.dataWindow();
            const s32          width      = dataWindow.max.x - dataWindow.min.x + 1;
            const s32          height     = dataWindow.max.y - dataWindow.min.y + 1;

            Imf::Array2D<Imf::Rgba> pixels(width, height);

            file.setFrameBuffer(&pixels[0][0], 1, width);
            file.readPixels(dataWindow.min.y, dataWindow.max.y);

            for (ssize x = 0; x < width; ++x)
            {
                for (ssize y = 0; y < height; ++y)
                {
                    auto& rgba = pixels[static_cast<s32>(x)][static_cast<s32>(y)];

                    auto Sanitize = [] (Imath::half& h)
                    {
                        if (h.isInfinity())
                        {
                            constexpr u16 F16_MAX = 0x7BFF;

                            h = Imath::half(Imath::half::FromBits, F16_MAX);
                        }

                        if (h.isNan())
                        {
                            h = Imath::half(Imath::half::FromBits, 0);
                        }
                    };

                    Sanitize(rgba.r);
                    Sanitize(rgba.g);
                    Sanitize(rgba.b);
                    Sanitize(rgba.a);
                }
            }

            const bool toF16           = (flags & ImageUploadFlags::F16)     == ImageUploadFlags::F16;
            const bool generateMipmaps = (flags & ImageUploadFlags::Mipmaps) == ImageUploadFlags::Mipmaps;

            const VkFormat format = toF16 ? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_R32G32B32A32_SFLOAT;

            const usize        texelCount = static_cast<usize>(width) * height;
            const usize        elemCount  = 4 * texelCount;
            const VkDeviceSize elemSize   = toF16 ? sizeof(f16) : sizeof(f32);
            const VkDeviceSize dataSize   = elemCount * elemSize;

            const auto stagingMemoryBlock = stagingPool.Allocate
            (
                device,
                allocator,
                dataSize,
                vkuFormatTexelBlockSize(format)
            );

            auto imageData = std::vector<u8>(dataSize);

            if (toF16)
            {
                std::memcpy(stagingMemoryBlock.hostAddress, &pixels[0][0], dataSize);
                std::memcpy(imageData.data(),           &pixels[0][0], dataSize);
            }
            else
            {
                SIMD::ConvertF16ToF32(reinterpret_cast<const f16*>(&pixels[0][0]), reinterpret_cast<f32*>(imageData.data()), elemCount);

                std::memcpy(stagingMemoryBlock.hostAddress, imageData.data(), dataSize);
            }

            const std::vector copyRegions = {VkBufferImageCopy2{
                .sType             = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
                .pNext             = nullptr,
                .bufferOffset      = stagingMemoryBlock.memoryBlock.offset,
                .bufferRowLength   = 0,
                .bufferImageHeight = 0,
                .imageSubresource  = {
                    .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel       = 0,
                    .baseArrayLayer = 0,
                    .layerCount     = 1
                },
                .imageOffset = {.x     = 0,                       .y      = 0,                        .z     = 0},
                .imageExtent = {.width = static_cast<u32>(width), .height = static_cast<u32>(height), .depth = 1}
            }};

            VkImageCreateInfo createInfo =
            {
                .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext                 = nullptr,
                .flags                 = 0,
                .imageType             = VK_IMAGE_TYPE_2D,
                .format                = format,
                .extent                = {.width = static_cast<u32>(width), .height = static_cast<u32>(height), .depth = 1},
                .mipLevels             = 1,
                .arrayLayers           = 1,
                .samples               = VK_SAMPLE_COUNT_1_BIT,
                .tiling                = VK_IMAGE_TILING_OPTIMAL,
                .usage                 = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 0,
                .pQueueFamilyIndices   = nullptr,
                .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED
            };

            if (generateMipmaps)
            {
                createInfo.mipLevels = static_cast<u32>(std::floor(std::log2(std::max(width, height))) + 1);
                createInfo.usage    |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            }

            const auto image = Vk::Image(allocator, createInfo, VK_IMAGE_ASPECT_COLOR_BIT);

            const auto imageView = Vk::ImageView
            (
                device,
                image,
                VK_IMAGE_VIEW_TYPE_2D,
                VkImageSubresourceRange{
                    .aspectMask     = image.aspect,
                    .baseMipLevel   = 0,
                    .levelCount     = image.mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount     = image.arrayLayers
                }
            );

            AppendUpload(Upload{
                .image           = image,
                .buffer          = stagingMemoryBlock.buffer,
                .copyRegions     = copyRegions,
                .srcStageMask    = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask   = VK_ACCESS_2_NONE,
                .oldLayout       = VK_IMAGE_LAYOUT_UNDEFINED,
                .generateMipmaps = generateMipmaps
            });

            deletionQueue.Push([&stagingPool, stagingMemoryBlock] () mutable
            {
                stagingPool.Free(stagingMemoryBlock);
            });

            executor.silent_async([width, height, format, imageData, generateMipmaps, cacheFile, hash] ()
            {
                constexpr std::array<VkDeviceSize, 1> TEXTURE_OFFSET_TABLE = {0};

                const auto textureOffsetTable = Cache::GenerateTextureOffsetTable(TEXTURE_OFFSET_TABLE);

                const u32 mipLevels = static_cast<u32>(std::floor(std::log2(std::max(width, height))) + 1);

                Cache::InsertIntoCache(Cache::Entry
                {
                    .cacheFile          = cacheFile,
                    .assetType          = Cache::AssetType::Texture,
                    .compressionType    = Cache::CompressionType::LZ4,
                    .assetHeader        = Cache::TextureHeader{
                        .width           = static_cast<u32>(width),
                        .height          = static_cast<u32>(height),
                        .mipLevels       = generateMipmaps ? mipLevels : 1,
                        .arrayLayers     = 1,
                        .faceCount       = 1,
                        .format          = format,
                        .generateMipmaps = generateMipmaps
                    },
                    .hash                 = hash,
                    .additionalHeaderData = textureOffsetTable,
                    .data                 = imageData,
                });
            });

            return Vk::UploadedImage
            {
                .image     = image,
                .imageView = imageView
            };
        }
        catch (const std::exception& e)
        {
            Logger::Error("Failed to load EXR file! [Error={}] [Path={}]\n", e.what(), path);
        }
    }

    Vk::UploadedImage ImageUploader::LoadKTX2File
    (
        VkDevice device,
        VmaAllocator allocator,
        Vk::StagingPool& stagingPool,
        tf::Executor& executor,
        Engine::DeletionQueue& deletionQueue,
        const std::string_view path
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        const auto cacheFile = GetImageUploadCacheFileName(path);

        const usize hash = Files::GetLastWriteTime(path);

        const Cache::Query query =
        {
            .cachedFile = cacheFile,
            .assetType  = Cache::AssetType::Texture,
            .hash       = hash
        };

        if (Cache::IsInCache(query))
        {
            const auto cache = Vk::ImageUploadCache
            {
                .name       = Files::GetNameWithoutExtension(path),
                .cachedPath = cacheFile
            };

            return LoadCache
            (
                device,
                allocator,
                stagingPool,
                deletionQueue,
                cache
            );
        }

        ktxTexture2* pTexture = nullptr;

        auto result = ktxTexture2_CreateFromNamedFile
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
            Logger::Error("{}\n", "Videos are not supported!");
        }

        if (ktxTexture2_NeedsTranscoding(pTexture))
        {
            #ifdef ENGINE_PROFILE
            ZoneScopedN("TranscodeBasis");
            #endif

            const u32 components = ktxTexture2_GetNumComponents(pTexture);

            ktx_transcode_fmt_e transcodeFormat = KTX_TTF_BC7_RGBA;

            if (components == 1)
            {
                transcodeFormat = KTX_TTF_BC4_R;
            }
            else if (components == 2)
            {
                transcodeFormat = KTX_TTF_BC5_RG;
            }

            result = ktxTexture2_TranscodeBasis(pTexture, transcodeFormat, 0);

            if (result != KTX_SUCCESS)
            {
                Logger::Error("Failed to transcode to BC7! [Error={}]", ktxErrorString(result));
            }
        }
        else
        {
            #ifdef ENGINE_PROFILE
            ZoneScopedN("Load Image Data");
            #endif

            result = ktxTexture2_LoadImageData(pTexture, nullptr, 0);

            if (result != KTX_SUCCESS)
            {
                Logger::Error("Failed to load image data! [Error={}]", ktxErrorString(result));
            }
        }

        const auto uploadedImage = LoadKTX2Internal
        (
            device,
            allocator,
            stagingPool,
            deletionQueue,
            pTexture
        );

        executor.silent_async([pTexture, cacheFile, hash] ()
        {
            auto imageData = std::vector<u8>(pTexture->dataSize);

            std::memcpy(imageData.data(), pTexture->pData, pTexture->dataSize);

            std::vector<VkDeviceSize> textureOffsetTable = {};

            for (u32 mipLevel = 0; mipLevel < pTexture->numLevels; ++mipLevel)
            {
                for (u32 arrayLayer = 0; arrayLayer < pTexture->numLayers; ++arrayLayer)
                {
                    for (u32 face = 0; face < pTexture->numFaces; ++face)
                    {
                        ktx_size_t offset = 0;

                        ktxTexture2_GetImageOffset
                        (
                            pTexture,
                            mipLevel,
                            arrayLayer,
                            face,
                            &offset
                        );

                        textureOffsetTable.emplace_back(offset);
                    }
                }
            }

            const auto textureOffsetTableAsBytes = Cache::GenerateTextureOffsetTable(textureOffsetTable);

            const Cache::Entry cacheEntry =
            {
                .cacheFile          = cacheFile,
                .assetType          = Cache::AssetType::Texture,
                .compressionType    = Cache::CompressionType::LZ4,
                .assetHeader        = Cache::TextureHeader{
                    .width           = pTexture->baseWidth,
                    .height          = pTexture->baseHeight,
                    .mipLevels       = pTexture->numLevels,
                    .arrayLayers     = pTexture->numLayers,
                    .faceCount       = pTexture->numFaces,
                    .format          = static_cast<VkFormat>(pTexture->vkFormat),
                    .generateMipmaps = pTexture->generateMipmaps
                },
                .hash                 = hash,
                .additionalHeaderData = textureOffsetTableAsBytes,
                .data                 = imageData,
            };

            ktxTexture2_Destroy(pTexture);

            Cache::InsertIntoCache(cacheEntry);
        });

        return uploadedImage;
    }

    Vk::UploadedImage ImageUploader::LoadKTX2Memory
    (
        VkDevice device,
        VmaAllocator allocator,
        Vk::StagingPool& stagingPool,
        Engine::DeletionQueue& deletionQueue,
        const Vk::ImageUploadMemory& memory
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        ktxTexture2* pTexture = nullptr;

        auto result = ktxTexture2_CreateFromMemory
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

        if (pTexture->isVideo)
        {
            Logger::Error("{}\n", "Videos are not supported!");
        }

        if (ktxTexture2_NeedsTranscoding(pTexture))
        {
            #ifdef ENGINE_PROFILE
            ZoneScopedN("TranscodeBasis");
            #endif

            const u32 components = ktxTexture2_GetNumComponents(pTexture);

            ktx_transcode_fmt_e transcodeFormat = KTX_TTF_BC7_RGBA;

            if (components == 1)
            {
                transcodeFormat = KTX_TTF_BC4_R;
            }
            else if (components == 2)
            {
                transcodeFormat = KTX_TTF_BC5_RG;
            }

            result = ktxTexture2_TranscodeBasis(pTexture, transcodeFormat, 0);

            if (result != KTX_SUCCESS)
            {
                Logger::Error("Failed to transcode! [Error={}]", ktxErrorString(result));
            }
        }
        else
        {
            #ifdef ENGINE_PROFILE
            ZoneScopedN("Load Image Data");
            #endif

            result = ktxTexture2_LoadImageData(pTexture, nullptr, 0);

            if (result != KTX_SUCCESS)
            {
                Logger::Error("Failed to load image data! [Error={}]", ktxErrorString(result));
            }
        }

        const auto uploadedImage = LoadKTX2Internal
        (
            device,
            allocator,
            stagingPool,
            deletionQueue,
            pTexture
        );

        ktxTexture2_Destroy(pTexture);

        return uploadedImage;
    }

    Vk::UploadedImage ImageUploader::LoadKTX2Internal
    (
        VkDevice device,
        VmaAllocator allocator,
        Vk::StagingPool& stagingPool,
        Engine::DeletionQueue& deletionQueue,
        ktxTexture2* pTexture
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        const auto stagingMemoryBlock = stagingPool.Allocate
        (
            device,
            allocator,
            pTexture->dataSize,
            vkuFormatTexelBlockSize(static_cast<VkFormat>(pTexture->vkFormat))
        );

        std::vector<VkBufferImageCopy2> copyRegions = {};

        // Copy
        {
            #ifdef ENGINE_PROFILE
            ZoneScopedN("Copy KTX2 To Staging Memory");
            #endif

            std::memcpy(stagingMemoryBlock.hostAddress, pTexture->pData, pTexture->dataSize);

            for (u32 mipLevel = 0; mipLevel < pTexture->numLevels; ++mipLevel)
            {
                const u32 mipWidth  = std::max(pTexture->baseWidth  >> mipLevel, 1u);
                const u32 mipHeight = std::max(pTexture->baseHeight >> mipLevel, 1u);

                for (u32 arrayLayer = 0; arrayLayer < pTexture->numLayers; ++arrayLayer)
                {
                    for (u32 face = 0; face < pTexture->numFaces; ++face)
                    {
                        ktx_size_t offset = 0;

                        ktxTexture2_GetImageOffset
                        (
                            pTexture,
                            mipLevel,
                            arrayLayer,
                            face,
                            &offset
                        );

                        copyRegions.emplace_back(VkBufferImageCopy2{
                            .sType             = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
                            .pNext             = nullptr,
                            .bufferOffset      = stagingMemoryBlock.memoryBlock.offset + offset,
                            .bufferRowLength   = 0,
                            .bufferImageHeight = 0,
                            .imageSubresource  = {
                                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                                .mipLevel       = mipLevel,
                                .baseArrayLayer = pTexture->numFaces * arrayLayer + face,
                                .layerCount     = 1
                            },
                            .imageOffset       = {.x     = 0,        .y      = 0,         .z     = 0},
                            .imageExtent       = {.width = mipWidth, .height = mipHeight, .depth = 1}
                        });
                    }
                }
            }
        }

        VkImageCreateFlags flags         = 0;
        VkImageViewType    imageViewType = VK_IMAGE_VIEW_TYPE_2D;

        if (pTexture->isCubemap)
        {
            flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

            if (pTexture->isArray)
            {
                imageViewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
            }
            else
            {
                imageViewType = VK_IMAGE_VIEW_TYPE_CUBE;
            }
        }
        else if (pTexture->isArray)
        {
            imageViewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        }

        const auto image = Vk::Image
        (
            allocator,
            VkImageCreateInfo{
                .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext                 = nullptr,
                .flags                 = flags,
                .imageType             = VK_IMAGE_TYPE_2D,
                .format                = static_cast<VkFormat>(pTexture->vkFormat),
                .extent                = {.width = pTexture->baseWidth, .height = pTexture->baseHeight, .depth = 1},
                .mipLevels             = pTexture->numLevels,
                .arrayLayers           = pTexture->numLayers * pTexture->numFaces,
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

        const auto imageView = Vk::ImageView
        (
            device,
            image,
            imageViewType,
            VkImageSubresourceRange{
                .aspectMask     = image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = image.arrayLayers
            }
        );

        AppendUpload(Upload{
            .image           = image,
            .buffer          = stagingMemoryBlock.buffer,
            .copyRegions     = copyRegions,
            .srcStageMask    = VK_PIPELINE_STAGE_2_NONE,
            .srcAccessMask   = VK_ACCESS_2_NONE,
            .oldLayout       = VK_IMAGE_LAYOUT_UNDEFINED,
            .generateMipmaps = pTexture->generateMipmaps
        });

        deletionQueue.Push([&stagingPool, stagingMemoryBlock] () mutable
        {
            stagingPool.Free(stagingMemoryBlock);
        });

        return Vk::UploadedImage
        {
            .image     = image,
            .imageView = imageView
        };
    }

    Vk::UploadedImage ImageUploader::LoadRawMemory
    (
        VkDevice device,
        VmaAllocator allocator,
        Vk::StagingPool& stagingPool,
        Engine::DeletionQueue& deletionQueue,
        const ImageUploadRawMemory& rawMemory,
        Vk::ImageUploadFlags flags
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        const bool generateMipmaps = (flags & ImageUploadFlags::Mipmaps) == ImageUploadFlags::Mipmaps;

        const VkDeviceSize dataSize = Vk::GetImageSize
        (
            rawMemory.format,
            rawMemory.width,
            rawMemory.height
        );

        const auto stagingMemoryBlock = stagingPool.Allocate
        (
            device,
            allocator,
            dataSize,
            vkuFormatTexelBlockSize(rawMemory.format)
        );

        std::memcpy(stagingMemoryBlock.hostAddress, rawMemory.data.data(), dataSize);

        std::vector<VkBufferImageCopy2> copyRegions = {};

        copyRegions.emplace_back(VkBufferImageCopy2{
            .sType             = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
            .pNext             = nullptr,
            .bufferOffset      = stagingMemoryBlock.memoryBlock.offset,
            .bufferRowLength   = 0,
            .bufferImageHeight = 0,
            .imageSubresource  = {
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel       = 0,
                .baseArrayLayer = 0,
                .layerCount     = 1
            },
            .imageOffset = {.x     = 0,               .y      = 0,                .z     = 0},
            .imageExtent = {.width = rawMemory.width, .height = rawMemory.height, .depth = 1}
        });

        VkImageCreateInfo createInfo =
        {
            .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .imageType             = VK_IMAGE_TYPE_2D,
            .format                = rawMemory.format,
            .extent                = {.width = rawMemory.width, .height = rawMemory.height, .depth = 1},
            .mipLevels             = 1,
            .arrayLayers           = 1,
            .samples               = VK_SAMPLE_COUNT_1_BIT,
            .tiling                = VK_IMAGE_TILING_OPTIMAL,
            .usage                 = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices   = nullptr,
            .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED
        };

        if (generateMipmaps)
        {
            createInfo.mipLevels = static_cast<u32>(std::floor(std::log2(std::max(rawMemory.width, rawMemory.height))) + 1);
            createInfo.usage    |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        }

        const auto image = Vk::Image(allocator, createInfo, VK_IMAGE_ASPECT_COLOR_BIT);

        const auto imageView = Vk::ImageView
        (
            device,
            image,
            VK_IMAGE_VIEW_TYPE_2D,
            VkImageSubresourceRange{
                .aspectMask     = image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = image.arrayLayers
            }
        );

        AppendUpload(Upload{
            .image           = image,
            .buffer          = stagingMemoryBlock.buffer,
            .copyRegions     = copyRegions,
            .srcStageMask    = VK_PIPELINE_STAGE_2_NONE,
            .srcAccessMask   = VK_ACCESS_2_NONE,
            .oldLayout       = VK_IMAGE_LAYOUT_UNDEFINED,
            .generateMipmaps = generateMipmaps
        });

        deletionQueue.Push([&stagingPool, stagingMemoryBlock] () mutable
        {
            stagingPool.Free(stagingMemoryBlock);
        });

        return Vk::UploadedImage
        {
            .image     = image,
            .imageView = imageView
        };
    }

    Vk::UploadedImage ImageUploader::LoadCache
    (
        VkDevice device,
        VmaAllocator allocator,
        Vk::StagingPool& stagingPool,
        Engine::DeletionQueue& deletionQueue,
        const Vk::ImageUploadCache& cache
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneNamed(zone, true);
        zone.NameFmt("%s", std::string(Files::GetName(cache.cachedPath)).c_str());
        #endif

        const auto cacheEntry  = Cache::GetFromCache(cache.cachedPath);
        const auto header      = std::get<Cache::TextureHeader>(cacheEntry.assetHeader);
        const auto offsetTable = Cache::ExtractTextureOffsetTable(cacheEntry.additionalHeaderData.value());

        const auto stagingMemoryBlock = stagingPool.Allocate
        (
            device,
            allocator,
            cacheEntry.data.size(),
            vkuFormatTexelBlockSize(header.format)
        );

        std::memcpy(stagingMemoryBlock.hostAddress, cacheEntry.data.data(), cacheEntry.data.size());

        std::vector<VkBufferImageCopy2> copyRegions = {};

        const u32 mipLevelsToCopy = header.generateMipmaps ? 1 : header.mipLevels;

        for (u32 mipLevel = 0; mipLevel < mipLevelsToCopy; ++mipLevel)
        {
            const u32 mipWidth  = std::max(header.width  >> mipLevel, 1u);
            const u32 mipHeight = std::max(header.height >> mipLevel, 1u);

            for (u32 arrayLayer = 0; arrayLayer < header.arrayLayers; ++arrayLayer)
            {
                for (u32 face = 0; face < header.faceCount; ++face)
                {
                    const usize index = (mipLevel * header.arrayLayers * header.faceCount) + (arrayLayer * header.faceCount) + face;

                    const VkDeviceSize offset = offsetTable[index];

                    copyRegions.emplace_back(VkBufferImageCopy2{
                        .sType             = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
                        .pNext             = nullptr,
                        .bufferOffset      = stagingMemoryBlock.memoryBlock.offset + offset,
                        .bufferRowLength   = 0,
                        .bufferImageHeight = 0,
                        .imageSubresource  = {
                            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                            .mipLevel       = mipLevel,
                            .baseArrayLayer = header.faceCount * arrayLayer + face,
                            .layerCount     = 1
                        },
                        .imageOffset       = {.x     = 0,        .y      = 0,         .z     = 0},
                        .imageExtent       = {.width = mipWidth, .height = mipHeight, .depth = 1}
                    });
                }
            }
        }

        const bool isCubemap = header.faceCount > 1;
        const bool isArray   = header.arrayLayers > 1;

        VkImageCreateFlags flags         = 0;
        VkImageViewType    imageViewType = VK_IMAGE_VIEW_TYPE_2D;

        if (isCubemap)
        {
            flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

            if (isArray)
            {
                imageViewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
            }
            else
            {
                imageViewType = VK_IMAGE_VIEW_TYPE_CUBE;
            }
        }
        else if (isArray)
        {
            imageViewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        }

        VkImageCreateInfo createInfo =
        {
            .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = flags,
            .imageType             = VK_IMAGE_TYPE_2D,
            .format                = header.format,
            .extent                = {.width = header.width, .height = header.height, .depth = 1},
            .mipLevels             = header.mipLevels,
            .arrayLayers           = header.arrayLayers * header.faceCount,
            .samples               = VK_SAMPLE_COUNT_1_BIT,
            .tiling                = VK_IMAGE_TILING_OPTIMAL,
            .usage                 = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices   = nullptr,
            .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED
        };

        if (header.generateMipmaps)
        {
            createInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        }

        const auto image = Vk::Image(allocator, createInfo, VK_IMAGE_ASPECT_COLOR_BIT);

        const auto imageView = Vk::ImageView
        (
            device,
            image,
            imageViewType,
            VkImageSubresourceRange{
                .aspectMask     = image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = image.arrayLayers
            }
        );

        AppendUpload(Upload{
            .image           = image,
            .buffer          = stagingMemoryBlock.buffer,
            .copyRegions     = copyRegions,
            .srcStageMask    = VK_PIPELINE_STAGE_2_NONE,
            .srcAccessMask   = VK_ACCESS_2_NONE,
            .oldLayout       = VK_IMAGE_LAYOUT_UNDEFINED,
            .generateMipmaps = header.generateMipmaps
        });

        deletionQueue.Push([&stagingPool, stagingMemoryBlock] () mutable
        {
            stagingPool.Free(stagingMemoryBlock);
        });

        return Vk::UploadedImage
        {
            .image     = image,
            .imageView = imageView
        };
    }

    void ImageUploader::AppendUpload(Upload&& upload)
    {
        const std::scoped_lock lock{m_mutex};

        m_pendingUploads.emplace_back(std::move(upload));
    }
}