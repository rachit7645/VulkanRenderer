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

#include "Swapchain.h"

#include <vulkan/vk_enum_string_helper.h>

#include "BarrierWriter.h"
#include "DebugUtils.h"
#include "ImmediateSubmit.h"
#include "Util.h"
#include "Util/Log.h"

namespace Vk
{
    Swapchain::Swapchain(const glm::ivec2& size, const Vk::Context& context)
    {
        if (!IsSurfaceValid(size, context))
        {
            Logger::Error("{}\n", "Invalid surface!");
        }

        CreateSwapChain(context);
        CreateStaticSyncObjects(context.device);
    }

    bool Swapchain::IsSurfaceValid(const glm::ivec2& size, const Vk::Context& context)
    {
        m_swapChainInfo = SwapchainInfo(context.physicalDevice, context.surface);
        extent          = ChooseSwapchainExtent(size);

        return extent.width != 0 && extent.height != 0;
    }

    void Swapchain::RecreateSwapChain(const Vk::Context& context)
    {
        DestroySwapchainResources(context.device);
        CreateSwapChain(context);
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

        const VkSwapchainPresentFenceInfoKHR fenceInfo =
        {
            .sType          = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_KHR,
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

    void Swapchain::CreateSwapChain(const Vk::Context& context)
    {
        surfaceFormat = ChooseSurfaceFormat();
        presentMode   = ChoosePresentationMode();

        u32 imageCount = GetImageCount();

        constexpr VkSwapchainPresentScalingCreateInfoKHR presentScalingCreateInfo =
        {
            .sType           = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_SCALING_CREATE_INFO_KHR,
            .pNext           = nullptr,
            .scalingBehavior = VK_PRESENT_SCALING_ASPECT_RATIO_STRETCH_BIT_KHR,
            .presentGravityX = VK_PRESENT_GRAVITY_MIN_BIT_KHR,
            .presentGravityY = VK_PRESENT_GRAVITY_MIN_BIT_KHR
        };

        const VkSwapchainPresentModesCreateInfoKHR presentModesCreateInfo =
        {
            .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODES_CREATE_INFO_KHR,
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
            .imageFormat           = surfaceFormat.format,
            .imageColorSpace       = surfaceFormat.colorSpace,
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

        auto imageHandles = std::vector<VkImage>(imageCount);

        Vk::CheckResult(vkGetSwapchainImagesKHR(
            context.device,
            handle,
            &imageCount,
            imageHandles.data()),
            "Failed to get swapchain images!"
        );

        images.resize(imageHandles.size());
        imageViews.resize(imageHandles.size());
        imageLayouts.resize(imageHandles.size());

        for (usize i = 0; i < imageHandles.size(); ++i)
        {
            images[i] = Vk::Image
            (
                imageHandles[i],
                extent.width,
                extent.height,
                1,
                1,
                1,
                surfaceFormat.format,
                VK_IMAGE_ASPECT_COLOR_BIT
            );

            imageViews[i] = Vk::ImageView
            (
                context.device,
                images[i],
                VK_IMAGE_VIEW_TYPE_2D,
                VkImageSubresourceRange{
                    .aspectMask     = images[i].aspect,
                    .baseMipLevel   = 0,
                    .levelCount     = images[i].mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount     = images[i].arrayLayers
                }
            );

            imageLayouts[i] = VK_IMAGE_LAYOUT_UNDEFINED;

            Vk::SetDebugName(context.device, images[i].handle,     fmt::format("Swapchain/Image{}", i));
            Vk::SetDebugName(context.device, imageViews[i].handle, fmt::format("Swapchain/ImageView{}", i));
        }

        CreateSyncObjects(context.device);
    }

    void Swapchain::CreateStaticSyncObjects(VkDevice device)
    {
        constexpr VkSemaphoreTypeCreateInfo semaphoreTypeCreateInfo =
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
        constexpr VkFenceCreateInfo fenceInfo =
        {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };

        constexpr VkSemaphoreTypeCreateInfo semaphoreTypeCreateInfo =
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
    }

    VkSurfaceFormatKHR Swapchain::ChooseSurfaceFormat() const
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
                const auto& currentSurfaceFormat = format2.surfaceFormat;

                if (currentSurfaceFormat.format == format &&
                    currentSurfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                {
                    return currentSurfaceFormat;
                }
            }
        }

        // Fallback #1 -> Any format with VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        for (const auto& format2 : formats)
        {
            if (format2.surfaceFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return format2.surfaceFormat;
            }
        }

        // Fallback #2 -> Return first available format (probably won't work)
        return formats[0].surfaceFormat;
    }

    VkPresentModeKHR Swapchain::ChoosePresentationMode() const
    {
        const auto& presentModes = m_swapChainInfo.presentModes;

        // FIFO is guaranteed to be supported (Lame)
        VkPresentModeKHR currentPresentMode = VK_PRESENT_MODE_FIFO_KHR;

        for (const auto availablePresentMode : presentModes)
        {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                currentPresentMode = availablePresentMode;
                break;
            }
        }

        return currentPresentMode;
    }

    VkExtent2D Swapchain::ChooseSwapchainExtent(const glm::uvec2& size) const
    {
        const auto& capabilities = m_swapChainInfo.capabilities.surfaceCapabilities;

        // Special Case: If current extent is not (0xFFFFFFFF, 0xFFFFFFFF), use surface size as swapchain extent
        if (capabilities.currentExtent.width  != std::numeric_limits<u32>::max() &&
            capabilities.currentExtent.height != std::numeric_limits<u32>::max())
        {
            return capabilities.currentExtent;
        }

        const auto minSize = glm::vk_cast(capabilities.minImageExtent);
        const auto maxSize = glm::vk_cast(capabilities.maxImageExtent);

        const auto actualExtent = glm::clamp(size, minSize, maxSize);

        return VkExtent2D
        {
            .width  = static_cast<u32>(actualExtent.x),
            .height = static_cast<u32>(actualExtent.y),
        };
    }

    u32 Swapchain::GetImageCount() const
    {
        const u32 minRequiredImages = std::max
        (
            m_swapChainInfo.capabilities.surfaceCapabilities.minImageCount,
            static_cast<u32>(Vk::FRAMES_IN_FLIGHT)
        );

        const u32 maxAllowedImages = m_swapChainInfo.capabilities.surfaceCapabilities.maxImageCount;

        u32 imageCount = 0;

        // Special case: If max image count is zero then there is no cap on imageCount
        if (maxAllowedImages == 0)
        {
            imageCount = minRequiredImages + 1;
        }
        else
        {
            imageCount = std::min(minRequiredImages + 1, maxAllowedImages);
        }

        return imageCount;
    }

    void Swapchain::DestroySwapchainResources(VkDevice device)
    {
        Vk::CheckResult(vkWaitForFences(
            device,
            presentFences.size(),
            presentFences.data(),
            VK_TRUE,
            std::numeric_limits<u64>::max()),
            "Failed to wait for fences!"
        );

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
        imageLayouts.clear();
        presentFences.clear();
        renderFinishedSemaphores.clear();
    }

    void Swapchain::Destroy(VkDevice device)
    {
        DestroySwapchainResources(device);

        vkDestroySwapchainKHR(device, handle, nullptr);

        for (const auto semaphore : imageAvailableSemaphores)
        {
            vkDestroySemaphore(device, semaphore, nullptr);
        }
    }
}
