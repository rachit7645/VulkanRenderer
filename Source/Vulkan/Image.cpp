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

#include "Image.h"

#include <vulkan/vk_enum_string_helper.h>

#include "Util.h"
#include "Util/Log.h"
#include "Vulkan/BarrierWriter.h"

namespace Vk
{
    Image::Image(VmaAllocator allocator, const VkImageCreateInfo& createInfo, VkImageAspectFlags aspect)
        : Image
          (
              VK_NULL_HANDLE,
              createInfo.extent.width,
              createInfo.extent.height,
              createInfo.extent.depth,
              createInfo.mipLevels,
              createInfo.arrayLayers,
              createInfo.format,
              createInfo.usage,
              aspect
          )
    {
        const VmaAllocationCreateInfo allocInfo =
        {
            .flags          = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT,
            .usage          = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            .requiredFlags  = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            .preferredFlags = 0,
            .memoryTypeBits = 0,
            .pool           = VK_NULL_HANDLE,
            .pUserData      = nullptr,
            .priority       = 0.0f
        };

        Vk::CheckResult(vmaCreateImage(
            allocator,
            &createInfo,
            &allocInfo,
            &handle,
            &allocation,
            &allocationInfo),
            "Failed to create image!"
        );
    }

    Image::Image
    (
        VkImage image,
        u32 width,
        u32 height,
        u32 depth,
        u32 mipLevels,
        u32 arrayLayers,
        VkFormat format,
        VkImageUsageFlags usage,
        VkImageAspectFlags aspect
    )
        : handle(image),
          width(width),
          height(height),
          depth(depth),
          mipLevels(mipLevels),
          arrayLayers(arrayLayers),
          format(format),
          usage(usage),
          aspect(aspect)
    {
    }

    bool Image::operator==(const Image& rhs) const
    {
        return handle == rhs.handle &&
               allocation == rhs.allocation &&
               width  == rhs.width &&
               height == rhs.height &&
               depth == rhs.depth &&
               mipLevels == rhs.mipLevels &&
               arrayLayers == rhs.arrayLayers &&
               format == rhs.format &&
               usage == rhs.usage &&
               aspect == rhs.aspect;
    }

    void Image::Barrier(const Vk::CommandBuffer& cmdBuffer, const Vk::ImageBarrier& barrier) const
    {
        const VkImageMemoryBarrier2 imageBarrier =
        {
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext               = nullptr,
            .srcStageMask        = barrier.srcStageMask,
            .srcAccessMask       = barrier.srcAccessMask,
            .dstStageMask        = barrier.dstStageMask,
            .dstAccessMask       = barrier.dstAccessMask,
            .oldLayout           = barrier.oldLayout,
            .newLayout           = barrier.newLayout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = handle,
            .subresourceRange    = {
                .aspectMask     = aspect,
                .baseMipLevel   = barrier.baseMipLevel,
                .levelCount     = barrier.levelCount,
                .baseArrayLayer = barrier.baseArrayLayer,
                .layerCount     = barrier.layerCount,
            }
        };

        const VkDependencyInfo dependencyInfo =
        {
            .sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext                    = nullptr,
            .dependencyFlags          = 0,
            .memoryBarrierCount       = 0,
            .pMemoryBarriers          = nullptr,
            .bufferMemoryBarrierCount = 0,
            .pBufferMemoryBarriers    = nullptr,
            .imageMemoryBarrierCount  = 1,
            .pImageMemoryBarriers     = &imageBarrier
        };

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencyInfo);
    }

    void Image::GenerateMipmaps(const Vk::CommandBuffer& cmdBuffer) const
    {
        if (mipLevels <= 1)
        {
            Barrier
            (
                cmdBuffer,
                Vk::ImageBarrier{
                    .srcStageMask   = VK_PIPELINE_STAGE_2_COPY_BIT,
                    .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                    .oldLayout      = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .baseMipLevel   = 0,
                    .levelCount     = mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount     = arrayLayers
                }
            );

            return;
        }

        for (u32 i = 0; i < arrayLayers; ++i)
        {
            s32 mipWidth  = static_cast<s32>(width);
            s32 mipHeight = static_cast<s32>(height);

            for (u32 j = 1; j < mipLevels; ++j)
            {
                Barrier
                (
                    cmdBuffer,
                    Vk::ImageBarrier{
                        .srcStageMask   = VK_PIPELINE_STAGE_2_BLIT_BIT,
                        .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                        .dstStageMask   = VK_PIPELINE_STAGE_2_BLIT_BIT,
                        .dstAccessMask  = VK_ACCESS_2_TRANSFER_READ_BIT,
                        .oldLayout      = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        .newLayout      = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                        .baseMipLevel   = j - 1,
                        .levelCount     = 1,
                        .baseArrayLayer = i,
                        .layerCount     = 1
                    }
                );

                const VkImageBlit2 blitRegion =
                {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
                    .pNext = nullptr,
                    .srcSubresource = {
                        .aspectMask     = aspect,
                        .mipLevel       = j - 1,
                        .baseArrayLayer = i,
                        .layerCount     = 1
                    },
                    .srcOffsets = {
                        {0, 0, 0},
                        {mipWidth, mipHeight, 1}
                    },
                    .dstSubresource = {
                        .aspectMask     = aspect,
                        .mipLevel       = j,
                        .baseArrayLayer = i,
                        .layerCount     = 1
                    },
                    .dstOffsets = {
                        {0, 0, 0},
                        {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1}
                    }
                };

                const VkBlitImageInfo2 blitInfo =
                {
                    .sType          = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
                    .pNext          = nullptr,
                    .srcImage       = handle,
                    .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    .dstImage       = handle,
                    .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .regionCount    = 1,
                    .pRegions       = &blitRegion,
                    .filter         = VK_FILTER_LINEAR
                };

                vkCmdBlitImage2(cmdBuffer.handle, &blitInfo);

                if (mipWidth > 1) mipWidth /= 2;
                if (mipHeight > 1) mipHeight /= 2;
            }
        }

        // Level 0 to Level (mipLevels - 1)
        Vk::BarrierWriter{}
        .WriteImageBarrier(
            *this,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_BLIT_BIT,
                .srcAccessMask  = VK_ACCESS_2_TRANSFER_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = mipLevels - 1,
                .baseArrayLayer = 0,
                .layerCount     = arrayLayers
            }
        )
        .WriteImageBarrier(
            *this,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_BLIT_BIT,
                .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = mipLevels - 1,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = arrayLayers
            }
        )
        .Execute(cmdBuffer);
    }

    void Image::Destroy(VmaAllocator allocator) const
    {
        if (handle == VK_NULL_HANDLE)
        {
            return;
        }

        vmaDestroyImage(allocator, handle, allocation);
    }
}
