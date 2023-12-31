/*
 *    Copyright 2023 - 2024 Rachit Khandelwal
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

#include "Swapchain.h"
#include "Util/Log.h"
#include "Util.h"

namespace Vk
{
    Swapchain::Swapchain(const std::shared_ptr<Engine::Window>& window, const std::shared_ptr<Vk::Context>& context)
    {
        CreateSwapChain(window, context);
        CreateImageViews(context->device);
        CreateSyncObjects(context->device);
        Logger::Info("Initialised swap chain! [handle={}]\n", reinterpret_cast<void*>(handle));
    }

    void Swapchain::RecreateSwapChain(const std::shared_ptr<Engine::Window>& window, const std::shared_ptr<Vk::Context>& context)
    {
        DestroySwapchain(context->device);
        window->WaitForRestoration();

        CreateSwapChain(window, context);
        CreateImageViews(context->device);

        Logger::Info("Recreated swap chain! [handle={}]\n", reinterpret_cast<void*>(handle));
    }

    void Swapchain::Present(VkQueue queue, usize FIF)
    {
        std::array<VkSemaphore,    1> signalSemaphores = {renderFinishedSemaphores[FIF]};
        std::array<VkSwapchainKHR, 1> swapChains       = {handle};
        std::array<u32,            1> imageIndices     = {imageIndex};

        VkPresentInfoKHR presentInfo =
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

        m_status[1] = vkQueuePresentKHR(queue, &presentInfo);
    }

    bool Swapchain::IsSwapchainValid()
    {
        bool isValid = m_status[0] == VK_SUCCESS && m_status[1] == VK_SUCCESS;
        m_status.fill(VK_SUCCESS);
        return isValid;
    }

    void Swapchain::AcquireSwapChainImage(VkDevice device, usize FIF)
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

        m_status[0] = vkAcquireNextImage2KHR
        (
            device,
            &acquireNextImageInfo,
            &imageIndex
        );
    }

    void Swapchain::CreateSwapChain(const std::shared_ptr<Engine::Window>& window, const std::shared_ptr<Vk::Context>& context)
    {
        m_swapChainInfo = SwapchainInfo(context->physicalDevice, context->surface);
        extent          = ChooseSwapExtent(window->handle);

        VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat();
        VkPresentModeKHR   presentMode   = ChoosePresentationMode();

        // Try to allocate 1 more than the min
        u32 imageCount = glm::min
        (
            m_swapChainInfo.capabilities.minImageCount + 1,
            m_swapChainInfo.capabilities.maxImageCount
        );

        VkSwapchainCreateInfoKHR createInfo =
        {
            .sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext                 = nullptr,
            .flags                 = 0,
            .surface               = context->surface,
            .minImageCount         = imageCount,
            .imageFormat           = surfaceFormat.format,
            .imageColorSpace       = surfaceFormat.colorSpace,
            .imageExtent           = extent,
            .imageArrayLayers      = 1,
            .imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices   = nullptr,
            .preTransform          = m_swapChainInfo.capabilities.currentTransform,
            .compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode           = presentMode,
            .clipped               = VK_TRUE,
            .oldSwapchain          = VK_NULL_HANDLE
        };

        Vk::CheckResult(vkCreateSwapchainKHR(
            context->device,
            &createInfo,
            nullptr,
            &handle),
            "Failed to create swap chain!"
        );

        Vk::CheckResult(vkGetSwapchainImagesKHR
        (
            context->device,
            handle,
            &imageCount,
            nullptr),
            "Failed to get swapchain image count!"
        );

        auto _images = std::vector<VkImage>(imageCount);
        Vk::CheckResult(vkGetSwapchainImagesKHR
        (
            context->device,
            handle,
            &imageCount,
            _images.data()),
            "Failed to get swapchain images!"
        );

        imageFormat = surfaceFormat.format;

        for (auto image : _images)
        {
            images.emplace_back
            (
                image,
                extent.width,
                extent.height,
                1,
                imageFormat,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_ASPECT_COLOR_BIT
            );
        }

        // Transition for presentation
        Vk::ImmediateSubmit(context, [this] (const Vk::CommandBuffer& cmdBuffer)
        {
            for (const auto& image : images)
            {
                image.TransitionLayout
                (
                    cmdBuffer,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
                );
            }
        });
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
        }

        Logger::Debug("{}\n", "Created swapchain sync objects!");
    }

    void Swapchain::CreateImageViews(VkDevice device)
    {
        for (const auto& image : images)
        {
            imageViews.emplace_back
            (
                device,
                image,
                VK_IMAGE_VIEW_TYPE_2D,
                imageFormat,
                VK_IMAGE_ASPECT_COLOR_BIT,
                0,
                1,
                0,
                1
            );
        }
    }

    VkSurfaceFormatKHR Swapchain::ChooseSurfaceFormat() const
    {
        const auto& formats = m_swapChainInfo.formats;

        for (const auto& availableFormat : formats)
        {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && // BGRA is faster or something IDK
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) // SRGB Buffer
            {
                return availableFormat;
            }
        }

        // By default, return the first format available (probably rgba or something)
        return formats[0];
    }

    VkPresentModeKHR Swapchain::ChoosePresentationMode() const
    {
        const auto& presentModes = m_swapChainInfo.presentModes;

        for (auto presentMode : presentModes)
        {
            // Mailbox my beloved
            if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                return presentMode;
            }
        }

        // FIFO is guaranteed to be supported (Lame)
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D Swapchain::ChooseSwapExtent(SDL_Window* window) const
    {
        const auto& capabilities = m_swapChainInfo.capabilities;

        // Some platforms set swap extents themselves
        if (capabilities.currentExtent.width != std::numeric_limits<u32>::max())
        {
            return capabilities.currentExtent;
        }

        glm::ivec2 size = {};
        SDL_Vulkan_GetDrawableSize(window, &size.x, &size.y);

        auto minSize = glm::ivec2(capabilities.minImageExtent.width, capabilities.minImageExtent.height);
        auto maxSize = glm::ivec2(capabilities.maxImageExtent.width, capabilities.maxImageExtent.height);

        auto actualExtent = glm::clamp(size, minSize, maxSize);

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

        vkDestroySwapchainKHR(device, handle, nullptr);

        images.clear();
        imageViews.clear();
    }

    void Swapchain::Destroy(VkDevice device)
    {
        Logger::Debug("{}\n", "Destroying swapchain!");

        DestroySwapchain(device);

        for (usize i = 0; i < FRAMES_IN_FLIGHT; ++i)
        {
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        }
    }
}