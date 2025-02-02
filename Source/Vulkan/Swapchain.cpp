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

#include <volk/volk.h>

#include "Vulkan/DebugUtils.h"
#include "Util/Log.h"
#include "Util.h"

namespace Vk
{
    Swapchain::Swapchain(const glm::ivec2& size, const Vk::Context& context)
    {
        CreateSwapChain(size, context);
        CreateSyncObjects(context.device);
        Logger::Info("Initialised swap chain! [handle={}]\n", std::bit_cast<void*>(handle));
    }

    void Swapchain::RecreateSwapChain(const glm::ivec2& size, const Vk::Context& context)
    {
        DestroySwapchain(context.device);
        CreateSwapChain(size, context);

        Logger::Info("Recreated swap chain! [handle={}]\n", std::bit_cast<void*>(handle));
    }

    VkResult Swapchain::Present(VkQueue queue, usize FIF)
    {
        const std::array signalSemaphores = {renderFinishedSemaphores[FIF]};
        const std::array swapChains       = {handle};
        const std::array imageIndices     = {imageIndex};

        const VkPresentInfoKHR presentInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext              = nullptr,
            .waitSemaphoreCount = static_cast<u32>(signalSemaphores.size()),
            .pWaitSemaphores    = signalSemaphores.data(),
            .swapchainCount     = static_cast<u32>(swapChains.size()),
            .pSwapchains        = swapChains.data(),
            .pImageIndices      = imageIndices.data(),
            .pResults           = nullptr
        };

        return vkQueuePresentKHR(queue, &presentInfo);
    }

    VkResult Swapchain::AcquireSwapChainImage(VkDevice device, usize FIF)
    {
        VkAcquireNextImageInfoKHR acquireNextImageInfo =
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

    void Swapchain::CreateSwapChain(const glm::ivec2& size, const Vk::Context& context)
    {
        m_swapChainInfo = SwapchainInfo(context.physicalDevice, context.surface);
        extent          = ChooseSwapExtent(size);

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

        VkSwapchainCreateInfoKHR createInfo =
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
            .imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
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

        vkDestroySwapchainKHR(context.device, createInfo.oldSwapchain, nullptr);

        Vk::CheckResult(vkGetSwapchainImagesKHR
        (
            context.device,
            handle,
            &imageCount,
            nullptr),
            "Failed to get swapchain image count!"
        );

        if (imageCount == 0)
        {
            Logger::VulkanError
            (
                "Failed to get any swapchain images! [handle={}] [device={}]\n",
                std::bit_cast<void*>(handle),
                std::bit_cast<void*>(context.device)
            );
        }

        auto _images = std::vector<VkImage>(imageCount);
        Vk::CheckResult(vkGetSwapchainImagesKHR
        (
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
                imageFormat,
                VK_IMAGE_ASPECT_COLOR_BIT
            );

            imageViews[i] = Vk::ImageView
            (
                context.device,
                images[i],
                VK_IMAGE_VIEW_TYPE_2D,
                imageFormat,
                {
                    .aspectMask     = images[i].aspect,
                    .baseMipLevel   = 0,
                    .levelCount     = images[i].mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount     = 1
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
            context.commandPool,
            [&] (const Vk::CommandBuffer& cmdBuffer)
            {
                for (auto&& image : images)
                {
                    image.Barrier
                    (
                        cmdBuffer,
                        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                        VK_ACCESS_2_NONE,
                        VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                        VK_ACCESS_2_NONE,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                        {
                            .aspectMask     = image.aspect,
                            .baseMipLevel   = 0,
                            .levelCount     = image.mipLevels,
                            .baseArrayLayer = 0,
                            .layerCount     = 1
                        }
                    );
                }
            }
        );
    }

    void Swapchain::CreateSyncObjects(VkDevice device)
    {
        VkSemaphoreCreateInfo semaphoreInfo =
        {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
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

            Vk::CheckResult(vkCreateSemaphore(
                device,
                &semaphoreInfo,
                nullptr,
                &renderFinishedSemaphores[i]),
                "Failed to create render semaphore!"
            );

            Vk::SetDebugName(device, imageAvailableSemaphores[i], fmt::format("Swapchain/ImageAvailableSemaphore{}", i));
            Vk::SetDebugName(device, renderFinishedSemaphores[i], fmt::format("Swapchain/RenderFinishedSemaphore{}", i));
        }

        Logger::Debug("{}\n", "Created swapchain sync objects!");
    }

    VkSurfaceFormat2KHR Swapchain::ChooseSurfaceFormat() const
    {
        const auto& formats = m_swapChainInfo.formats;

        for (const auto& availableFormat : formats)
        {
            if (availableFormat.surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB && // BGRA is faster or something IDK
                availableFormat.surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) // SRGB Buffer
            {
                Logger::Debug
                (
                    "Choosing surface format! [Format={}] [ColorSpace={}]\n",
                    string_VkFormat(availableFormat.surfaceFormat.format),
                    string_VkColorSpaceKHR(availableFormat.surfaceFormat.colorSpace)
                );

                return availableFormat;
            }
        }

        // By default, return the first format available (probably rgba or something)
        return formats[0];
    }

    VkPresentModeKHR Swapchain::ChoosePresentationMode() const
    {
        const auto& presentModes = m_swapChainInfo.presentModes;

        for (const auto presentMode : presentModes)
        {
            // Mailbox my beloved
            if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                Logger::Debug
                (
                    "Choosing presentation mode! [PresentMode={}]\n",
                    string_VkPresentModeKHR(presentMode)
                );

                return presentMode;
            }
        }

        // FIFO is guaranteed to be supported (Lame)
        return VK_PRESENT_MODE_FIFO_KHR;
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

        auto actualExtent = glm::clamp(size, minSize, maxSize);

        Logger::Debug("Choosing swap extent! [X={}] [Y={}]\n", actualExtent.x, actualExtent.y);

        return
        {
            .width  = static_cast<u32>(actualExtent.x),
            .height = static_cast<u32>(actualExtent.y),
        };
    }

    void Swapchain::DestroySwapchain(VkDevice device)
    {
        for (auto&& imageView : imageViews)
        {
            imageView.Destroy(device);
        }

        images.clear();
        imageViews.clear();
    }

    void Swapchain::Destroy(VkDevice device)
    {
        Logger::Debug("{}\n", "Destroying swapchain!");

        DestroySwapchain(device);

        vkDestroySwapchainKHR(device, handle, nullptr);

        for (usize i = 0; i < FRAMES_IN_FLIGHT; ++i)
        {
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        }
    }
}