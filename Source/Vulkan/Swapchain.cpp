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

#include "Swapchain.h"

#include "DebugUtils.h"
#include "ImmediateSubmit.h"
#include "Util.h"
#include "BarrierWriter.h"
#include "Util/Log.h"

namespace Vk
{
    Swapchain::Swapchain(const glm::ivec2& size, const Vk::Context& context, Vk::CommandBufferAllocator& cmdBufferAllocator)
    {
        if (!IsSurfaceValid(size, context))
        {
            Logger::Error("{]\n", "Invalid surface!");
        }

        CreateSwapChain(context, cmdBufferAllocator);
        CreateStaticSyncObjects(context.device);

        Logger::Info("Initialised swap chain! [handle={}]\n", std::bit_cast<void*>(handle));
    }

    bool Swapchain::IsSurfaceValid(const glm::ivec2& size, const Vk::Context& context)
    {
        m_swapChainInfo = SwapchainInfo(context.physicalDevice, context.surface);
        extent          = ChooseSwapExtent(size);

        if (extent.width == 0 || extent.height == 0)
        {
            return false;
        }

        return true;
    }

    void Swapchain::RecreateSwapChain(const Vk::Context& context, Vk::CommandBufferAllocator& cmdBufferAllocator)
    {
        DestroySwapchain(context.device);
        CreateSwapChain(context, cmdBufferAllocator);

        Logger::Info("Recreated swap chain! [handle={}]\n", std::bit_cast<void*>(handle));
    }

    VkResult Swapchain::Present(VkDevice device, VkQueue queue)
    {
        Vk::CheckResult(vkWaitForFences(
            device,
            1,
            &presentFences[imageIndex],
            VK_TRUE,
            std::numeric_limits<u64>::max()),
            "Failed to wait for fence!"
        );

        Vk::CheckResult(vkResetFences(
            device,
            1,
            &presentFences[imageIndex]),
            "Unable to reset fence!"
        );

        const VkSwapchainPresentFenceInfoEXT fenceInfo =
        {
            .sType          = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT,
            .pNext          = nullptr,
            .swapchainCount = 1,
            .pFences        = &presentFences[imageIndex]
        };

        const VkPresentInfoKHR presentInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext              = &fenceInfo,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = &renderFinishedSemaphores[imageIndex],
            .swapchainCount     = 1,
            .pSwapchains        = &handle,
            .pImageIndices      = &imageIndex,
            .pResults           = nullptr
        };

        return vkQueuePresentKHR(queue, &presentInfo);
    }

    VkResult Swapchain::AcquireSwapChainImage(VkDevice device, usize FIF)
    {
        const VkAcquireNextImageInfoKHR acquireNextImageInfo =
        {
            .sType      = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
            .pNext      = nullptr,
            .swapchain  = handle,
            .timeout    = std::numeric_limits<u64>::max(),
            .semaphore  = imageAvailableSemaphores[FIF],
            .fence      = VK_NULL_HANDLE,
            .deviceMask = 1
        };

        return vkAcquireNextImage2KHR
        (
            device,
            &acquireNextImageInfo,
            &imageIndex
        );
    }

    void Swapchain::Blit(const Vk::CommandBuffer& cmdBuffer, const Vk::Image& finalColor)
    {
        const auto& swapchainImage = images[imageIndex];

        Vk::BarrierWriter barrierWriter = {};

        barrierWriter
        .WriteImageBarrier(
            finalColor,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask  = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_BLIT_BIT,
                .dstAccessMask  = VK_ACCESS_2_TRANSFER_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .baseMipLevel   = 0,
                .levelCount     = finalColor.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = finalColor.arrayLayers
            }
        )
        .WriteImageBarrier(
            swapchainImage,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_NONE,
                .srcAccessMask  = VK_ACCESS_2_NONE,
                .dstStageMask   = VK_PIPELINE_STAGE_2_BLIT_BIT,
                .dstAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .newLayout      = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .baseMipLevel   = 0,
                .levelCount     = swapchainImage.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = swapchainImage.arrayLayers
            }
        )
        .Execute(cmdBuffer);

        const VkImageBlit2 blitRegion =
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
            .pNext = nullptr,
            .srcSubresource = {
                .aspectMask     = finalColor.aspect,
                .mipLevel       = 0,
                .baseArrayLayer = 0,
                .layerCount     = finalColor.arrayLayers
            },
            .srcOffsets = {
                {0, 0, 0},
                {static_cast<s32>(finalColor.width), static_cast<s32>(finalColor.height), 1}
            },
            .dstSubresource = {
                .aspectMask     = swapchainImage.aspect,
                .mipLevel       = 0,
                .baseArrayLayer = 0,
                .layerCount     = swapchainImage.arrayLayers
            },
            .dstOffsets = {
                {0, 0, 0},
                {static_cast<s32>(swapchainImage.width), static_cast<s32>(swapchainImage.height), 1}
            }
        };

        const VkBlitImageInfo2 blitImageInfo =
        {
            .sType          = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
            .pNext          = nullptr,
            .srcImage       = finalColor.handle,
            .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .dstImage       = swapchainImage.handle,
            .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .regionCount    = 1,
            .pRegions       = &blitRegion,
            .filter         = VK_FILTER_LINEAR
        };

        vkCmdBlitImage2(cmdBuffer.handle, &blitImageInfo);

        barrierWriter
        .WriteImageBarrier(
            finalColor,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_BLIT_BIT,
                .srcAccessMask  = VK_ACCESS_2_TRANSFER_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .baseMipLevel   = 0,
                .levelCount     = finalColor.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = finalColor.arrayLayers
            }
        )
        .WriteImageBarrier(
            swapchainImage,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_BLIT_BIT,
                .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstAccessMask  = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .baseMipLevel   = 0,
                .levelCount     = swapchainImage.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = swapchainImage.arrayLayers
            }
        )
        .Execute(cmdBuffer);
    }

    void Swapchain::CreateSwapChain(const Vk::Context& context, Vk::CommandBufferAllocator& cmdBufferAllocator)
    {
        const VkSurfaceFormat2KHR surfaceFormat = ChooseSurfaceFormat();
        const VkPresentModeKHR    presentMode   = ChoosePresentationMode();

        // Try to allocate 1 more than the min
        u32 imageCount = std::min
        (
            m_swapChainInfo.capabilities.surfaceCapabilities.minImageCount + 1,
            m_swapChainInfo.capabilities.surfaceCapabilities.maxImageCount
        );

        const VkSwapchainPresentScalingCreateInfoEXT presentScalingCreateInfo =
        {
            .sType           = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_SCALING_CREATE_INFO_EXT,
            .pNext           = nullptr,
            .scalingBehavior = VK_PRESENT_SCALING_ASPECT_RATIO_STRETCH_BIT_EXT,
            .presentGravityX = VK_PRESENT_GRAVITY_MIN_BIT_EXT,
            .presentGravityY = VK_PRESENT_GRAVITY_MIN_BIT_EXT
        };

        const VkSwapchainPresentModesCreateInfoEXT presentModesCreateInfo =
        {
            .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODES_CREATE_INFO_EXT,
            .pNext            = &presentScalingCreateInfo,
            .presentModeCount = 1,
            .pPresentModes    = &presentMode
        };

        const VkSwapchainCreateInfoKHR createInfo =
        {
            .sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext                 = &presentModesCreateInfo,
            .flags                 = 0,
            .surface               = context.surface,
            .minImageCount         = imageCount,
            .imageFormat           = surfaceFormat.surfaceFormat.format,
            .imageColorSpace       = surfaceFormat.surfaceFormat.colorSpace,
            .imageExtent           = extent,
            .imageArrayLayers      = 1,
            .imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices   = nullptr,
            .preTransform          = m_swapChainInfo.capabilities.surfaceCapabilities.currentTransform,
            .compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode           = presentMode,
            .clipped               = VK_TRUE,
            .oldSwapchain          = handle
        };

        Vk::CheckResult(vkCreateSwapchainKHR(
            context.device,
            &createInfo,
            nullptr,
            &handle),
            "Failed to create swap chain!"
        );

        Vk::SetDebugName(context.device, handle, "Swapchain");

        if (createInfo.oldSwapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(context.device, createInfo.oldSwapchain, nullptr);
        }

        Vk::CheckResult(vkGetSwapchainImagesKHR(
            context.device,
            handle,
            &imageCount,
            nullptr),
            "Failed to get swapchain image count!"
        );

        if (imageCount == 0)
        {
            Logger::Error
            (
                "Failed to get any swapchain images! [handle={}] [device={}]\n",
                std::bit_cast<void*>(handle),
                std::bit_cast<void*>(context.device)
            );
        }

        auto _images = std::vector<VkImage>(imageCount);
        Vk::CheckResult(vkGetSwapchainImagesKHR(
            context.device,
            handle,
            &imageCount,
            _images.data()),
            "Failed to get swapchain images!"
        );

        imageFormat = surfaceFormat.surfaceFormat.format;

        images.resize(_images.size());
        imageViews.resize(_images.size());

        for (usize i = 0; i < _images.size(); ++i)
        {
            images[i] = Vk::Image
            (
                _images[i],
                extent.width,
                extent.height,
                1,
                1,
                1,
                imageFormat,
                createInfo.imageUsage,
                VK_IMAGE_ASPECT_COLOR_BIT
            );

            imageViews[i] = Vk::ImageView
            (
                context.device,
                images[i],
                VK_IMAGE_VIEW_TYPE_2D,
                {
                    .aspectMask     = images[i].aspect,
                    .baseMipLevel   = 0,
                    .levelCount     = images[i].mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount     = images[i].mipLevels
                }
            );

            Vk::SetDebugName(context.device, images[i].handle,     fmt::format("Swapchain/Image{}", i));
            Vk::SetDebugName(context.device, imageViews[i].handle, fmt::format("Swapchain/ImageView{}", i));
        }

        // Transition for presentation
        Vk::ImmediateSubmit
        (
            context.device,
            context.graphicsQueue,
            cmdBufferAllocator,
            [&] (const Vk::CommandBuffer& cmdBuffer)
            {
                Vk::BarrierWriter barrierWriter = {};

                for (const auto& image : images)
                {
                    barrierWriter.WriteImageBarrier
                    (
                        image,
                        Vk::ImageBarrier{
                            .srcStageMask   = VK_PIPELINE_STAGE_2_NONE,
                            .srcAccessMask  = VK_ACCESS_2_NONE,
                            .dstStageMask   = VK_PIPELINE_STAGE_2_NONE,
                            .dstAccessMask  = VK_ACCESS_2_NONE,
                            .oldLayout      = VK_IMAGE_LAYOUT_UNDEFINED,
                            .newLayout      = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                            .baseMipLevel   = 0,
                            .levelCount     = image.mipLevels,
                            .baseArrayLayer = 0,
                            .layerCount     = image.arrayLayers
                        }
                    );
                }

                barrierWriter.Execute(cmdBuffer);
            }
        );

        CreateSyncObjects(context.device);
    }

    void Swapchain::CreateStaticSyncObjects(VkDevice device)
    {
        const VkSemaphoreTypeCreateInfo semaphoreTypeCreateInfo =
        {
            .sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
            .pNext         = nullptr,
            .semaphoreType = VK_SEMAPHORE_TYPE_BINARY,
            .initialValue  = 0
        };

        const VkSemaphoreCreateInfo semaphoreInfo =
        {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = &semaphoreTypeCreateInfo,
            .flags = 0
        };

        for (usize i = 0; i < FRAMES_IN_FLIGHT; ++i)
        {
            Vk::CheckResult(vkCreateSemaphore(
                device,
                &semaphoreInfo,
                nullptr,
                &imageAvailableSemaphores[i]),
                "Failed to create image semaphore!"
            );

            Vk::SetDebugName(device, imageAvailableSemaphores[i], fmt::format("Swapchain/ImageAvailableSemaphore{}", i));
        }
    }

    void Swapchain::CreateSyncObjects(VkDevice device)
    {
        const VkFenceCreateInfo fenceInfo =
        {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };

        const VkSemaphoreTypeCreateInfo semaphoreTypeCreateInfo =
        {
            .sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
            .pNext         = nullptr,
            .semaphoreType = VK_SEMAPHORE_TYPE_BINARY,
            .initialValue  = 0
        };

        const VkSemaphoreCreateInfo semaphoreInfo =
        {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = &semaphoreTypeCreateInfo,
            .flags = 0
        };

        presentFences.resize(images.size());
        renderFinishedSemaphores.resize(images.size());

        for (usize i = 0; i < images.size(); ++i)
        {
            Vk::CheckResult(vkCreateFence(
                device,
                &fenceInfo,
                nullptr,
                &presentFences[i]),
                "Failed to create present fence!"
            );

            Vk::CheckResult(vkCreateSemaphore(
                device,
                &semaphoreInfo,
                nullptr,
                &renderFinishedSemaphores[i]),
                "Failed to create render semaphore!"
            );

            Vk::SetDebugName(device, presentFences[i],            fmt::format("Swapchain/PresentFence{}",            i));
            Vk::SetDebugName(device, renderFinishedSemaphores[i], fmt::format("Swapchain/RenderFinishedSemaphore{}", i));
        }

        Logger::Debug("{}\n", "Created swapchain sync objects!");
    }

    VkSurfaceFormat2KHR Swapchain::ChooseSurfaceFormat() const
    {
        const auto& formats = m_swapChainInfo.formats;

        constexpr std::array PREFERRED_FORMATS =
        {
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_FORMAT_B8G8R8A8_SRGB,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_B8G8R8A8_UNORM
        };

        // Really, there's no way this is gonna happen lol
        [[unlikely]] if (formats.empty())
        {
            Logger::Error("{}\n", "No surface formats found!");
        }

        // Check preferred formats first
        for (const auto format : PREFERRED_FORMATS)
        {
            for (const auto& format2 : formats)
            {
                const auto& surfaceFormat = format2.surfaceFormat;

                if (surfaceFormat.format == format &&
                    surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                {
                    Logger::Debug
                    (
                        "Choosing surface format! [Format={}] [ColorSpace={}]\n",
                        string_VkFormat(surfaceFormat.format),
                        string_VkColorSpaceKHR(surfaceFormat.colorSpace)
                    );

                    return format2;
                }
            }
        }

        // Fallback #1 -> Any format with VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        for (const auto& format2 : formats)
        {
            if (format2.surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                Logger::Debug
                (
                    "Choosing surface format! [Fallback #1] [Format={}] [ColorSpace={}]\n",
                    string_VkFormat(format2.surfaceFormat.format),
                    string_VkColorSpaceKHR(format2.surfaceFormat.colorSpace)
                );

                return format2;
            }
        }

        // Fallback #2 -> Return first available format (probably won't work)
        Logger::Debug
        (
            "Choosing surface format! [Fallback #2] [Format={}] [ColorSpace={}]\n",
            string_VkFormat(formats[0].surfaceFormat.format),
            string_VkColorSpaceKHR(formats[0].surfaceFormat.colorSpace)
        );

        return formats[0];
    }

    VkPresentModeKHR Swapchain::ChoosePresentationMode() const
    {
        const auto& presentModes = m_swapChainInfo.presentModes;

        // FIFO is guaranteed to be supported (Lame)
        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

        for (const auto availablePresentMode : presentModes)
        {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                presentMode = availablePresentMode;
            }
        }

        Logger::Debug
        (
            "Choosing presentation mode! [PresentMode={}]\n",
            string_VkPresentModeKHR(presentMode)
        );

        return presentMode;
    }

    VkExtent2D Swapchain::ChooseSwapExtent(const glm::ivec2& size) const
    {
        const auto& capabilities = m_swapChainInfo.capabilities;

        // Some platforms set swap extents themselves
        if (capabilities.surfaceCapabilities.currentExtent.width != std::numeric_limits<u32>::max())
        {
            Logger::Debug
            (
                "Choosing existing swap extent! [width={}] [height={}]\n",
                capabilities.surfaceCapabilities.currentExtent.width,
                capabilities.surfaceCapabilities.currentExtent.height
            );

            return capabilities.surfaceCapabilities.currentExtent;
        }

        const auto minSize = glm::ivec2(capabilities.surfaceCapabilities.minImageExtent.width, capabilities.surfaceCapabilities.minImageExtent.height);
        const auto maxSize = glm::ivec2(capabilities.surfaceCapabilities.maxImageExtent.width, capabilities.surfaceCapabilities.maxImageExtent.height);

        const auto actualExtent = glm::clamp(size, minSize, maxSize);

        Logger::Debug("Choosing swap extent! [X={}] [Y={}]\n", actualExtent.x, actualExtent.y);

        return
        {
            .width  = static_cast<u32>(actualExtent.x),
            .height = static_cast<u32>(actualExtent.y),
        };
    }

    void Swapchain::DestroySwapchain(VkDevice device)
    {
        for (auto& imageView : imageViews)
        {
            imageView.Destroy(device);
        }

        for (const auto fence : presentFences)
        {
            vkDestroyFence(device, fence, nullptr);
        }

        for (const auto semaphore : renderFinishedSemaphores)
        {
            vkDestroySemaphore(device, semaphore, nullptr);
        }

        images.clear();
        imageViews.clear();
    }

    void Swapchain::Destroy(VkDevice device)
    {
        Logger::Debug("{}\n", "Destroying swapchain!");

        DestroySwapchain(device);

        vkDestroySwapchainKHR(device, handle, nullptr);

        for (const auto semaphore : imageAvailableSemaphores)
        {
            vkDestroySemaphore(device, semaphore, nullptr);
        }
    }
}
