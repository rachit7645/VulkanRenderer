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

#include "Swapchain.h"
#include "Util/Log.h"

namespace Vk
{
    Swapchain::Swapchain(const std::shared_ptr<Engine::Window>& window, const std::shared_ptr<Vk::Context>& context)
    {
        // Create swap chain
        CreateSwapChain(window, context);
        CreateImageViews(context->device);
        CreateSyncObjects(context->device);
        // Log
        Logger::Info("Initialised swap chain! [handle={}]\n", reinterpret_cast<void*>(handle));
    }

    void Swapchain::RecreateSwapChain(const std::shared_ptr<Engine::Window>& window, const std::shared_ptr<Vk::Context>& context)
    {
        // Clean up old swap chain
        DestroySwapchain(context->device);
        // Wait
        window->WaitForRestoration();
        // Create new swap chain
        CreateSwapChain(window, context);
        CreateImageViews(context->device);
        // Log
        Logger::Info("Recreated swap chain! [handle={}]\n", reinterpret_cast<void*>(handle));
    }

    void Swapchain::Present(VkQueue queue, usize FIF)
    {
        // Signal semaphores
        std::array<VkSemaphore, 1> signalSemaphores = {renderFinishedSemaphores[FIF]};
        // Swap chains
        std::array<VkSwapchainKHR, 1> swapChains = {handle};
        // Image indices
        std::array<u32, 1> imageIndices = {imageIndex};

        // Presentation info
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

        // Present
        m_status[1] = vkQueuePresentKHR(queue, &presentInfo);
    }

    bool Swapchain::IsSwapchainValid()
    {
        // Check if swapchain is valid
        bool isValid = !std::ranges::any_of(m_status, [](const auto& result)
        {
            // Return
            return (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || result != VK_SUCCESS);
        });
        // Reset state
        m_status.fill(VK_SUCCESS);
        // Return
        return isValid;
    }

    void Swapchain::AcquireSwapChainImage(VkDevice device, usize FIF)
    {
        // Acquire image info
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

        // Acquire
        m_status[0] = vkAcquireNextImage2KHR
        (
            device,
            &acquireNextImageInfo,
            &imageIndex
        );
    }

    void Swapchain::CreateSwapChain(const std::shared_ptr<Engine::Window>& window, const std::shared_ptr<Vk::Context>& context)
    {
        // Get swap chain info
        m_swapChainInfo = SwapchainInfo(context->physicalDevice, context->surface);

        // Get swap chain config data
        VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat();
        VkPresentModeKHR   presentMode   = ChoosePresentationMode();
        // Get extent
        extent = ChooseSwapExtent(window->handle);

        // Get image count
        u32 imageCount = glm::min
        (
            m_swapChainInfo.capabilities.minImageCount + 1,
            m_swapChainInfo.capabilities.maxImageCount
        );

        // Swap chain creation data
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

        // Create swap chain
        if (vkCreateSwapchainKHR(
                context->device,
                &createInfo,
                nullptr,
                &handle
            ) != VK_SUCCESS)
        {
            // Log
            Logger::Error("Failed to create swap chain! [device={}]\n", reinterpret_cast<void*>(context->device));
        }

        // Temporary image vector
        std::vector<VkImage> _images = {};
        // Get image count
        vkGetSwapchainImagesKHR
        (
            context->device,
            handle,
            &imageCount,
            nullptr
        );
        // Resize
        _images.resize(imageCount);
        // Get the images
        vkGetSwapchainImagesKHR
        (
            context->device,
            handle,
            &imageCount,
            _images.data()
        );

        // Store other properties
        imageFormat = surfaceFormat.format;

        // Convert images
        for (auto image : _images)
        {
            // Create and add to vector
            m_images.emplace_back
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
    }

    void Swapchain::CreateSyncObjects(VkDevice device)
    {
        // Semaphore info
        VkSemaphoreCreateInfo semaphoreInfo =
        {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0
        };

        // For each frame in flight
        for (usize i = 0; i < FRAMES_IN_FLIGHT; ++i)
        {
            if
            (
                vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                    &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                    &renderFinishedSemaphores[i]) != VK_SUCCESS
            )
            {
                // Log
                Logger::Error("{}\n", "Failed to create swapchain sync objects!");
            }
        }

        // Log
        Logger::Debug("{}\n", "Created swapchain sync objects!");
    }

    void Swapchain::CreateImageViews(VkDevice device)
    {
        // Loop over all swap chain images
        for (const auto& image : m_images)
        {
            // Create view
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
        // Formats
        const auto& formats = m_swapChainInfo.formats;

        // Search
        for (const auto& availableFormat : formats)
        {
            // Check
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && // BGRA is faster or something IDK
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) // SRGB Buffer
            {
                // Found preferred format!
                return availableFormat;
            }
        }

        // By default, return the first format
        return formats[0];
    }

    VkPresentModeKHR Swapchain::ChoosePresentationMode() const
    {
        // Presentation modes
        const auto& presentModes = m_swapChainInfo.presentModes;

        // Check all presentation modes
        for (auto presentMode : presentModes)
        {
            // Check for mailbox
            if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                // Found it!
                return presentMode;
            }
        }

        // FIFO is guaranteed to be supported
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D Swapchain::ChooseSwapExtent(SDL_Window* window) const
    {
        // Surface capability data
        const auto& capabilities = m_swapChainInfo.capabilities;

        // Some platforms set swap extents themselves
        if (capabilities.currentExtent.width != std::numeric_limits<u32>::max())
        {
            // Just give back the original swap extent
            return capabilities.currentExtent;
        }

        // Window pixel size
        glm::ivec2 size = {};
        SDL_Vulkan_GetDrawableSize(window, &size.x, &size.y);

        // Get min and max
        auto minSize = glm::ivec2(capabilities.minImageExtent.width, capabilities.minImageExtent.height);
        auto maxSize = glm::ivec2(capabilities.maxImageExtent.width, capabilities.maxImageExtent.height);
        // Clamp
        auto actualExtent = glm::clamp(size, minSize, maxSize);

        // Return
        return
        {
            .width  = static_cast<u32>(actualExtent.x),
            .height = static_cast<u32>(actualExtent.y),
        };
    }

    void Swapchain::DestroySwapchain(VkDevice device)
    {
        // Destroy swap chain image views
        for (auto&& imageView : imageViews)
        {
            imageView.Destroy(device);
        }

        // Destroy swap chain
        vkDestroySwapchainKHR(device, handle, nullptr);

        // Clear
        m_images.clear();
        imageViews.clear();
    }

    void Swapchain::Destroy(VkDevice device)
    {
        // Log
        Logger::Debug("{}\n", "Destroying swapchain!");
        // Destroy swapchain
        DestroySwapchain(device);
        // Destroy sync objects
        for (usize i = 0; i < FRAMES_IN_FLIGHT; ++i)
        {
            // Destroy semaphores
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        }
    }
}