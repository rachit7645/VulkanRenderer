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
#include <volk/volk.h>

#include "Util.h"
#include "Util/Log.h"

namespace Vk
{
    Image::Image(VmaAllocator allocator, const VkImageCreateInfo& createInfo, VkImageAspectFlags aspect)
        : Image
          (
              VK_NULL_HANDLE,
              createInfo.extent.width,
              createInfo.extent.height,
              createInfo.mipLevels,
              createInfo.format,
              aspect
          )
    {
        const VmaAllocationCreateInfo allocInfo =
        {
            .flags          = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT,
            .usage          = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            #ifdef ENGINE_DEBUG
            .requiredFlags  = 0,
            .preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            #else
            .requiredFlags  = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            .preferredFlags = 0,
            #endif
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

        #ifdef ENGINE_DEBUG
        VkMemoryPropertyFlags flags;
        vmaGetMemoryTypeProperties(allocator, allocationInfo.memoryType, &flags);

        if (!(flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            Logger::Warning
            (
                "Image not allocated from device local memory! May harm performance. [handle={}] [allocation={}] [flags={}]\n",
                std::bit_cast<void*>(handle),
                std::bit_cast<void*>(allocation),
                string_VkMemoryPropertyFlags(flags)
            );
        }
        #endif

        Logger::Debug("Created image! [handle={}]\n", std::bit_cast<void*>(handle));
    }

    Image::Image
    (
        VkImage image,
        u32 width,
        u32 height,
        u32 mipLevels,
        VkFormat format,
        VkImageAspectFlags aspect
    )
        : handle(image),
          width(width),
          height(height),
          mipLevels(mipLevels),
          format(format),
          aspect(aspect)
    {
    }

    bool Image::operator==(const Image& rhs) const
    {
        return handle == rhs.handle &&
               allocation == rhs.allocation &&
               width  == rhs.width &&
               height == rhs.height &&
               format == rhs.format &&
               aspect == rhs.aspect;
    }

    void Image::Barrier
    (
        const Vk::CommandBuffer& cmdBuffer,
        VkPipelineStageFlags2 srcStageMask,
        VkAccessFlags2 srcAccessMask,
        VkPipelineStageFlags2 dstStageMask,
        VkAccessFlags2 dstAccessMask,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        const VkImageSubresourceRange& subresourceRange
    )
    {
        const VkImageMemoryBarrier2 barrier =
        {
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext               = nullptr,
            .srcStageMask        = srcStageMask,
            .srcAccessMask       = srcAccessMask,
            .dstStageMask        = dstStageMask,
            .dstAccessMask       = dstAccessMask,
            .oldLayout           = oldLayout,
            .newLayout           = newLayout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = handle,
            .subresourceRange    = subresourceRange
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
            .pImageMemoryBarriers     = &barrier
        };

        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencyInfo);
    }

    void Image::GenerateMipmaps(const Vk::CommandBuffer& cmdBuffer)
    {
        if (mipLevels <= 1)
        {
            Barrier
            (
                cmdBuffer,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                VK_ACCESS_2_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                {
                    .aspectMask     = aspect,
                    .baseMipLevel   = 0,
                    .levelCount     = mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount     = 1
                }
            );

            return;
        }

        s32 mipWidth  = static_cast<s32>(width);
        s32 mipHeight = static_cast<s32>(height);

        for (u32 i = 1; i < mipLevels; ++i)
        {
            Barrier
            (
                cmdBuffer,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                VK_ACCESS_2_TRANSFER_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                {
                    .aspectMask     = aspect,
                    .baseMipLevel   = i - 1,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1
                }
            );

            VkImageBlit2 blitRegion =
            {
                .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
                .pNext = nullptr,
                .srcSubresource = {
                    .aspectMask     = aspect,
                    .mipLevel       = i - 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1
                },
                .srcOffsets = {
                    {0, 0, 0},
                    {mipWidth, mipHeight, 1}
                },
                .dstSubresource = {
                    .aspectMask     = aspect,
                    .mipLevel       = i,
                    .baseArrayLayer = 0,
                    .layerCount     = 1
                },
                .dstOffsets = {
                    {0, 0, 0},
                    {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1}
                }
            };

            VkBlitImageInfo2 blitInfo =
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

        // Level 0 to Level (mipLevels - 1)
        Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_READ_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            {
                .aspectMask     = aspect,
                .baseMipLevel   = 0,
                .levelCount     = mipLevels - 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );


        // Final mip level
        Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            {
                .aspectMask     = aspect,
                .baseMipLevel   = mipLevels - 1,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );
    }

    void Image::Destroy(VmaAllocator allocator) const
    {
        Logger::Debug
        (
            "Destroying image! [handle={}] [allocation={}]\n",
            std::bit_cast<void*>(handle),
            std::bit_cast<void*>(allocation)
        );

        vmaDestroyImage(allocator, handle, allocation);
    }
}