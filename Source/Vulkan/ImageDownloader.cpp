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

#include "ImageDownloader.h"

#include "Engine/Cache.h"
#include "Vulkan/BarrierWriter.h"
#include "Vulkan/Constants.h"
#include "Vulkan/DebugUtils.h"

namespace Vk
{
    void ImageDownloader::RequestDownload(const Vk::ImageDownload& download)
    {
        requestedDownloads.emplace_back(download);
    }

    void ImageDownloader::Update
    (
        usize frameIndex,
        VkDevice device,
        VmaAllocator allocator,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::GraphicsTimeline& timeline,
        Scratch::Allocator& scratchAllocator,
        tf::Executor& executor
    )
    {
        for (auto iter = pendingDownloads.cbegin(); iter != pendingDownloads.cend(); )
        {
            const bool isReadbackReady = timeline.IsAtOrPastStage
            (
                iter->readbackFrameIndex + Vk::FRAMES_IN_FLIGHT,
                Vk::GraphicsTimeline::Stage::SwapchainImageAcquired,
                device
            );

            if (!isReadbackReady)
            {
                ++iter;

                continue;
            }

            executor.silent_async([allocator, pendingDownload = *iter] mutable
            {
                const u8* pMappedData = static_cast<u8*>(pendingDownload.readbackBuffer.hostAddress);

                const auto readbackData = std::vector(pMappedData, pMappedData + pendingDownload.readbackBuffer.size);

                pendingDownload.readbackBuffer.Destroy(allocator);

                if (pendingDownload.postDownloadAction != PostDownloadAction::Cache)
                {
                    Logger::Error("{}\n", "Invalid post download action!");
                };

                const auto postDownloadCache = std::get<Vk::PostDownloadCache>(pendingDownload.postDownloadActionData);

                // TODO: Add in missing logic
                ENGINE_ASSERT
                (
                    pendingDownload.image.mipLevels == 1 &&
                    pendingDownload.image.arrayLayers == 1 &&
                    pendingDownload.image.depth == 1 &&
                    "Invalid download parameters!"
                );

                constexpr std::array<VkDeviceSize, 1> TEXTURE_OFFSET_TABLE = {0};

                const auto textureOffsetTableAsBytes = Cache::GenerateTextureOffsetTable(TEXTURE_OFFSET_TABLE);

                Cache::InsertIntoCache(Cache::Entry
                {
                    .cacheFile          = postDownloadCache.cacheFile,
                    .assetType          = Cache::AssetType::Texture,
                    .compressionType    = Cache::CompressionType::LZ4,
                    .assetHeader        = Cache::TextureHeader{
                        .width           = pendingDownload.image.width,
                        .height          = pendingDownload.image.height,
                        .mipLevels       = 1,
                        .arrayLayers     = 1,
                        .faceCount       = 1,
                        .format          = pendingDownload.image.format,
                        .offsetTableSize = textureOffsetTableAsBytes.size(),
                    },
                    .hash               = postDownloadCache.hash,
                    .textureOffsetTable = textureOffsetTableAsBytes,
                    .data               = readbackData,
                });
            });

            iter = pendingDownloads.erase(iter);
        }

        const usize initialNewDownloadIndex = pendingDownloads.size();

        Vk::BeginLabel(cmdBuffer, "Setup Requested Image Downloads", {0.2098f, 0.9143f, 0.7859f, 1.0f});

        auto barrierWriter = Vk::ScratchBarrierWriter(scratchAllocator);

        for (const auto& requestedDownload : requestedDownloads)
        {
            const VkDeviceSize readbackSize = Vk::GetImageSize
            (
                requestedDownload.image.format,
                requestedDownload.image.width,
                requestedDownload.image.height
            );

            const auto readbackBuffer = Vk::Buffer
            (
               device,
               allocator,
               readbackSize,
               0,
               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
               VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
               VMA_MEMORY_USAGE_AUTO
            );

            barrierWriter
            .WriteImageBarrier(
                requestedDownload.image,
                Vk::ImageBarrier{
                    .srcStageMask    = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .srcAccessMask   = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                    .dstStageMask    = VK_PIPELINE_STAGE_2_COPY_BIT,
                    .dstAccessMask   = VK_ACCESS_2_TRANSFER_READ_BIT,
                    .oldLayout       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .newLayout       = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    .srcQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                    .baseMipLevel    = 0,
                    .levelCount      = requestedDownload.image.mipLevels,
                    .baseArrayLayer  = 0,
                    .layerCount      = requestedDownload.image.arrayLayers
                }
            )
            .WriteBufferBarrier(
                readbackBuffer,
                Vk::BufferBarrier{
                    .srcStageMask    = VK_PIPELINE_STAGE_2_NONE,
                    .srcAccessMask   = VK_ACCESS_2_NONE,
                    .dstStageMask    = VK_PIPELINE_STAGE_2_COPY_BIT,
                    .dstAccessMask   = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .srcQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                    .offset          = 0,
                    .size            = readbackBuffer.size
                }
            );

            pendingDownloads.emplace_back(ImageDownloader::PendingDownload{
                .image                  = requestedDownload.image,
                .postDownloadAction     = requestedDownload.postDownloadAction,
                .postDownloadActionData = requestedDownload.postDownloadActionData,
                .readbackBuffer         = readbackBuffer,
                .readbackFrameIndex     = frameIndex
            });
        }

        barrierWriter.Execute(cmdBuffer);

        for (usize i = initialNewDownloadIndex; i < pendingDownloads.size(); ++i)
        {
            const auto& pendingDownload = pendingDownloads[i];

            const VkBufferImageCopy2 copyRegion =
            {
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
                .imageOffset       = {.x     = 0,                           .y      = 0,                            .z     = 0},
                .imageExtent       = {.width = pendingDownload.image.width, .height = pendingDownload.image.height, .depth = 1}
            };

            const VkCopyImageToBufferInfo2 copyInfo =
            {
                .sType          = VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2,
                .pNext          = nullptr,
                .srcImage       = pendingDownload.image.handle,
                .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .dstBuffer      = pendingDownload.readbackBuffer.handle,
                .regionCount    = 1,
                .pRegions       = &copyRegion
            };

            vkCmdCopyImageToBuffer2(cmdBuffer.handle, &copyInfo);
        }

        for (usize i = initialNewDownloadIndex; i < pendingDownloads.size(); ++i)
        {
            const auto& pendingDownload = pendingDownloads[i];

            barrierWriter
            .WriteImageBarrier(
                pendingDownload.image,
                Vk::ImageBarrier{
                    .srcStageMask    = VK_PIPELINE_STAGE_2_COPY_BIT,
                    .srcAccessMask   = VK_ACCESS_2_TRANSFER_READ_BIT,
                    .dstStageMask    = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .dstAccessMask   = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                    .oldLayout       = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    .newLayout       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .srcQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily  = VK_QUEUE_FAMILY_IGNORED,
                    .baseMipLevel    = 0,
                    .levelCount      = pendingDownload.image.mipLevels,
                    .baseArrayLayer  = 0,
                    .layerCount      = pendingDownload.image.arrayLayers
                }
            );
        }

        barrierWriter.Execute(cmdBuffer);

        Vk::EndLabel(cmdBuffer);

        requestedDownloads.clear();
    }

    void ImageDownloader::Destroy(VmaAllocator allocator)
    {
        for (auto& download : pendingDownloads)
        {
            download.readbackBuffer.Destroy(allocator);
        }
    }
}
