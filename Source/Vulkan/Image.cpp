/*
 *    Copyright 2023 Rachit Khandelwal
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

#include <vulkan/vk_enum_string_helper.h>

#include "Image.h"
#include "Util.h"
#include "Util/Log.h"

namespace Vk
{
    Image::Image
    (
        const std::shared_ptr<Vk::Context>& context,
        u32 width,
        u32 height,
        u32 mipLevels,
        VkFormat format,
        VkImageTiling tiling,
        VkImageAspectFlags aspect,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties
    )
        : Image(VK_NULL_HANDLE, width, height, mipLevels, format, tiling, aspect)
    {
        CreateImage(context->allocator, usage, properties);
        Logger::Debug("Created image! [handle={}]\n", reinterpret_cast<void*>(handle));
    }

    Image::Image
    (
        VkImage image,
        u32 width,
        u32 height,
        u32 mipLevels,
        VkFormat format,
        VkImageTiling tiling,
        VkImageAspectFlags aspect
    )
        : handle(image),
          width(width),
          height(height),
          mipLevels(mipLevels),
          format(format),
          tiling(tiling),
          aspect(aspect)
    {
    }

    bool Image::operator==(const Image& rhs) const
    {
        return handle == rhs.handle &&
               allocation == rhs.allocation &&
               width  == rhs.width  &&
               height == rhs.height &&
               format == rhs.format &&
               tiling == rhs.tiling &&
               aspect == rhs.aspect;
    }

    void Image::CreateImage
    (
        VmaAllocator allocator,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties
    )
    {
        VkImageCreateInfo imageInfo =
        {
            .sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .imageType             = VK_IMAGE_TYPE_2D,
            .format                = format,
            .extent                = {width, height, 1},
            .mipLevels             = mipLevels,
            .arrayLayers           = 1,
            .samples               = VK_SAMPLE_COUNT_1_BIT,
            .tiling                = tiling,
            .usage                 = usage,
            .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = VK_QUEUE_FAMILY_IGNORED,
            .pQueueFamilyIndices   = nullptr,
            .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED
        };
        
        VmaAllocationCreateInfo allocInfo =
        {
            .flags          = 0,
            .usage          = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            .requiredFlags  = properties,
            .preferredFlags = 0,
            .memoryTypeBits = 0,
            .pool           = VK_NULL_HANDLE,
            .pUserData      = nullptr,
            .priority       = 0.0f
        };
        
        if (vmaCreateImage(
                allocator,
                &imageInfo,
                &allocInfo,
                &handle,
                &allocation,
                nullptr
            ) != VK_SUCCESS)
        {
            Logger::Error("Failed to create image! [allocator={}]\n",
                std::bit_cast<void*>(allocator)
            );
        }
    }

    void Image::TransitionLayout
    (
        const Vk::CommandBuffer& cmdBuffer,
        VkImageLayout oldLayout,
        VkImageLayout newLayout
    ) const
    {
        VkImageMemoryBarrier2 barrier =
        {
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .pNext               = nullptr,
            .srcStageMask        = VK_PIPELINE_STAGE_2_NONE,
            .srcAccessMask       = VK_ACCESS_2_NONE,
            .dstStageMask        = VK_PIPELINE_STAGE_2_NONE,
            .dstAccessMask       = VK_ACCESS_2_NONE,
            .oldLayout           = oldLayout,
            .newLayout           = newLayout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image               = handle,
            .subresourceRange    = {
                .aspectMask     = aspect,
                .baseMipLevel   = 0,
                .levelCount     = mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        };

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_2_NONE;
            barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;

            barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

            barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_2_NONE;
            barrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
            
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_2_NONE;
            barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
            
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
        {
            barrier.srcAccessMask = VK_ACCESS_2_NONE;
            barrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
            
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
        {
            barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
            
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
        }
        else
        {
            // Invalid
            Logger::VulkanError
            (
                "Invalid layout transition! [old={}] [new={}]\n",
                string_VkImageLayout(oldLayout),
                string_VkImageLayout(newLayout)
            );
        }

        // Dependency info
        VkDependencyInfo dependencyInfo =
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

        // Set barrier
        vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencyInfo);
    }

    void Image::CopyFromBuffer(const std::shared_ptr<Vk::Context>& context, Vk::Buffer& buffer)
    {
        Vk::ImmediateSubmit(context, [&](const Vk::CommandBuffer& cmdBuffer)
        {
            // Copy info
            VkBufferImageCopy2 copyRegion =
            {
                .sType             = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
                .pNext             = nullptr,
                .bufferOffset      = 0,
                .bufferRowLength   = 0,
                .bufferImageHeight = 0,
                .imageSubresource  = {
                    .aspectMask     = aspect,
                    .mipLevel       = 0,
                    .baseArrayLayer = 0,
                    .layerCount     = 1
                },
                .imageOffset       = {0, 0, 0},
                .imageExtent       = {width, height, 1}
            };

            // Copy info
            VkCopyBufferToImageInfo2 copyInfo =
            {
                .sType          = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
                .pNext          = nullptr,
                .srcBuffer      = buffer.handle,
                .dstImage       = handle,
                .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .regionCount    = 1,
                .pRegions       = &copyRegion
            };

            // Copy
            vkCmdCopyBufferToImage2(cmdBuffer.handle, &copyInfo);
        });
    }

    void Image::GenerateMipmaps(const std::shared_ptr<Vk::Context>& context)
    {
        Vk::ImmediateSubmit(context, [&] (const Vk::CommandBuffer& cmdBuffer)
        {
            // Barrier
            VkImageMemoryBarrier2 barrier =
            {
                .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .pNext               = nullptr,
                .srcStageMask        = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask       = VK_ACCESS_2_NONE,
                .dstStageMask        = VK_PIPELINE_STAGE_2_NONE,
                .dstAccessMask       = VK_ACCESS_2_NONE,
                .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image               = handle,
                .subresourceRange    = {
                    .aspectMask     = aspect,
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1
                }
            };

            // Dependency info
            VkDependencyInfo dependencyInfo =
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

            // Mip dimensions
            s32 mipWidth  = static_cast<s32>(width);
            s32 mipHeight = static_cast<s32>(height);

            // Loop over the rest of the mipmap chain
            for (u32 i = 1; i < mipLevels; ++i)
            {
                // Mipmap level
                barrier.subresourceRange.baseMipLevel = i - 1;
                // Layout
                barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                // Access mask
                barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
                // Stages
                barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
                barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;

                // Barrier
                vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencyInfo);

                // Image blit info
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

                // Blit info
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

                // Blit Image
                vkCmdBlitImage2(cmdBuffer.handle, &blitInfo);

                // Configure layouts
                barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                // Configure access masks
                barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
                barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
                // Stages
                barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
                barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

                // Barrier
                vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencyInfo);

                // Divide mip dimensions
                if (mipWidth > 1) mipWidth /= 2;
                if (mipHeight > 1) mipHeight /= 2;
            }

            // Base mipmap level
            barrier.subresourceRange.baseMipLevel = mipLevels - 1;
            // Configure layouts
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            // Configure access masks
            barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
            // Stages
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

            // Transition final level
            vkCmdPipelineBarrier2(cmdBuffer.handle, &dependencyInfo);
        });
    }

    void Image::Destroy(VmaAllocator allocator) const
    {
        // Log
        Logger::Debug
        (
            "Destroying image! [handle={}] [allocation={}]\n",
            reinterpret_cast<void*>(handle),
            reinterpret_cast<void*>(allocation)
        );
        // Destroy image
        vmaDestroyImage(allocator, handle, allocation);
    }
}