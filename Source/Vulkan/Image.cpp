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
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties
    )
        : width(width),
          height(height),
          format(format),
          tiling(tiling)
    {
        // Create image
        CreateImage(context, usage, properties);
        // Log
        Logger::Info("Created image! [handle={}]\n", reinterpret_cast<void*>(handle));
    }

    Image::Image(u32 width, u32 height, VkFormat format, VkImage image)
        : handle(image),
          width(width),
          height(height),
          format(format),
          tiling(VK_IMAGE_TILING_OPTIMAL)
    {
    }

    void Image::CreateImage
    (
        const std::shared_ptr<Vk::Context>& context,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties
    )
    {
        // Image creation info
        VkImageCreateInfo imageInfo =
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
            .tiling                = tiling,
            .usage                 = usage,
            .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = VK_QUEUE_FAMILY_IGNORED,
            .pQueueFamilyIndices   = nullptr,
            .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED
        };

        // Create image
        if (vkCreateImage(
                context->device,
                &imageInfo,
                nullptr,
                &handle
            ) != VK_SUCCESS)
        {
            // Log
            Logger::Error("Failed to create image! [device={}]\n",
                reinterpret_cast<void*>(context->device)
            );
        }

        // Get memory requirements
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(context->device, handle, &memRequirements);

        // Allocation info
        VkMemoryAllocateInfo allocInfo =
        {
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext           = nullptr,
            .allocationSize  = memRequirements.size,
            .memoryTypeIndex = Vk::FindMemoryType(
                memRequirements.memoryTypeBits,
                properties,
                context->phyMemProperties
            )
        };

        // Allocate
        if (vkAllocateMemory(
                context->device,
                &allocInfo,
                nullptr,
                &memory
            ) != VK_SUCCESS)
        {
            // Log
            Logger::Error
            (
                "Failed to allocate image memory! [device={}] [buffer={}]\n",
                reinterpret_cast<void*>(context->device),
                reinterpret_cast<void*>(handle)
            );
        }

        // Attach memory to image
        vkBindImageMemory(context->device, handle, memory, 0);
    }

    void Image::TransitionLayout
    (
        const std::shared_ptr<Vk::Context>& context,
        VkImageLayout oldLayout,
        VkImageLayout newLayout
    )
    {
        Vk::SingleTimeCmdBuffer(context, [&] (VkCommandBuffer cmdBuffer)
        {
            // Barrier data
            VkImageMemoryBarrier barrier =
            {
                .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext               = nullptr,
                .srcAccessMask       = VK_ACCESS_NONE,
                .dstAccessMask       = VK_ACCESS_NONE,
                .oldLayout           = oldLayout,
                .newLayout           = newLayout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image               = handle,
                .subresourceRange    = {
                    .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT, // FIXME: Make this a parameter
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1
                }
            };

            // Stages
            VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_NONE;
            VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_NONE;

            if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
            {
                // Access data
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                // Stage data
                srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            }
            else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            {
                // Access data
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                // Stage data
                srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            }
            else
            {
                // Invalid
                Logger::Error
                (
                    "Invalid layout transition! [old={}] [new={}]\n",
                    string_VkImageLayout(oldLayout),
                    string_VkImageLayout(newLayout)
                );
            }

            // Set barrier
            vkCmdPipelineBarrier
            (
                cmdBuffer,
                srcStage, dstStage,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );
        });
    }

    void Image::CopyFromBuffer(const std::shared_ptr<Vk::Context>& context, Vk::Buffer& buffer)
    {
        Vk::SingleTimeCmdBuffer(context, [&] (VkCommandBuffer cmdBuffer)
        {
            // Copy info
            VkBufferImageCopy copyRegion =
            {
                .bufferOffset      = 0,
                .bufferRowLength   = 0,
                .bufferImageHeight = 0,
                .imageSubresource  = {
                    .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT, // FIXME: Make this a parameter
                    .mipLevel       = 0,
                    .baseArrayLayer = 0,
                    .layerCount     = 1
                },
                .imageOffset       = {0, 0, 0},
                .imageExtent       = {width, height, 1}
            };
            // Copy
            vkCmdCopyBufferToImage
            (
                cmdBuffer,
                buffer.handle,
                handle,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &copyRegion
            );
        });
    }

    void Image::Destroy(VkDevice device)
    {
        // Destroy image
        vkDestroyImage(device, handle, nullptr);
        // Free associated memory
        vkFreeMemory(device, memory, nullptr);
    }
}