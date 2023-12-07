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
#include "Builders/RenderPassBuilder.h"
#include "Builders/SubpassBuilder.h"

namespace Vk
{
    Swapchain::Swapchain(const std::shared_ptr<Engine::Window>& window, const std::shared_ptr<Vk::Context>& context)
    {
        // Create swap chain
        CreateSwapChain(window, context);
        CreateImageViews(context->device);
        CreateDepthBuffer(context);
        CreateRenderPass(context->device);
        CreateFramebuffers(context->device);
        // Log
        Logger::Info("Initialised swap chain! [handle={}]\n", reinterpret_cast<void*>(handle));
    }

    void Swapchain::RecreateSwapChain(const std::shared_ptr<Engine::Window>& window, const std::shared_ptr<Vk::Context>& context)
    {
        // Wait for gpu to finish
        vkDeviceWaitIdle(context->device);
        // Clean up old swap chain
        DestroySwapChain(context->device);
        // Wait
        window->WaitForRestoration();
        // Create new swap chain
        CreateSwapChain(window, context);
        CreateImageViews(context->device);
        CreateDepthBuffer(context);
        CreateFramebuffers(context->device);
        // Log
        Logger::Info("Recreated swap chain! [handle={}]\n", reinterpret_cast<void*>(handle));
    }

    void Swapchain::Destroy(VkDevice device)
    {
        // Destroy main swapchain data
        DestroySwapChain(device);
        // Destroy renderpass
        vkDestroyRenderPass(device, renderPass, nullptr);
    }

    void Swapchain::DestroySwapChain(VkDevice device)
    {
        // Destroy framebuffers
        for (auto&& framebuffer : framebuffers)
        {
            framebuffer.Destroy(device);
        }

        // Destroy swap chain images
        for (auto&& imageView : m_imageViews)
        {
            imageView.Destroy(device);
        }

        // Destroy depth buffer
        depthBuffer.Destroy(device);
        // Destroy swap chain
        vkDestroySwapchainKHR(device, handle, nullptr);

        // Clear
        framebuffers.clear();
        m_images.clear();
        m_imageViews.clear();
    }

    void Swapchain::CreateSwapChain(const std::shared_ptr<Engine::Window>& window, const std::shared_ptr<Vk::Context>& context)
    {
        // Get swap chain info
        swapChainInfo = SwapchainInfo(context->physicalDevice, context->surface);

        // Get swap chain config data
        VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat();
        VkPresentModeKHR   presentMode   = ChoosePresentationMode();
        // Get extent
        extent = ChooseSwapExtent(window->handle);

        // Get image count
        u32 imageCount = glm::min
        (
            swapChainInfo.capabilities.minImageCount + 1,
            swapChainInfo.capabilities.maxImageCount
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
            .preTransform          = swapChainInfo.capabilities.currentTransform,
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
        m_imageFormat = surfaceFormat.format;

        // Convert images
        for (auto image : _images)
        {
            // Create and add to vector
            m_images.emplace_back
            (
                image,
                extent.width,
                extent.height,
                m_imageFormat,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_ASPECT_COLOR_BIT
            );
        }
    }

    void Swapchain::CreateImageViews(VkDevice device)
    {
        // Loop over all swap chain images
        for (const auto& image : m_images)
        {
            // Create view
            m_imageViews.emplace_back
            (
                device,
                image,
                VK_IMAGE_VIEW_TYPE_2D,
                m_imageFormat,
                VK_IMAGE_ASPECT_COLOR_BIT
            );
        }
    }

    void Swapchain::CreateDepthBuffer(const std::shared_ptr<Vk::Context>& context)
    {
        // Create
        depthBuffer = Vk::DepthBuffer(context, extent);
    }

    void Swapchain::CreateRenderPass(VkDevice device)
    {
        // Build
        renderPass = RenderPassBuilder::Create(device)
                    .AddAttachment(
                        m_imageFormat,
                        VK_SAMPLE_COUNT_1_BIT,
                        VK_ATTACHMENT_LOAD_OP_CLEAR,
                        VK_ATTACHMENT_STORE_OP_STORE,
                        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                        VK_ATTACHMENT_STORE_OP_DONT_CARE,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
                    .AddAttachment(
                        depthBuffer.depthImage.format,
                        VK_SAMPLE_COUNT_1_BIT,
                        VK_ATTACHMENT_LOAD_OP_CLEAR,
                        VK_ATTACHMENT_STORE_OP_DONT_CARE,
                        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                        VK_ATTACHMENT_STORE_OP_DONT_CARE,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                    .AddSubpass(
                        Vk::SubpassBuilder::Create()
                        .AddColorReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                        .AddDepthReference(1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                        .SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS)
                        .SetDependency(
                            VK_SUBPASS_EXTERNAL,
                            0,
                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                            0,
                            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)
                        .Build())
                    .Build();
    }

    void Swapchain::CreateFramebuffers(VkDevice device)
    {
        // Resize
        framebuffers.reserve(m_imageViews.size());

        // For each image view
        for (auto & m_imageView : m_imageViews)
        {
            // Attachments
            std::array<Vk::ImageView, 2> attachmentViews =
            {
                m_imageView,
                depthBuffer.depthImageView
            };
            // Create framebuffer
            framebuffers.emplace_back
            (
                device,
                renderPass,
                attachmentViews,
                glm::uvec2(extent.width, extent.height),
                1
            );
        }
    }

    VkSurfaceFormatKHR Swapchain::ChooseSurfaceFormat()
    {
        // Formats
        const auto& formats = swapChainInfo.formats;

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

    VkPresentModeKHR Swapchain::ChoosePresentationMode()
    {
        // Presentation modes
        const auto& presentModes = swapChainInfo.presentModes;

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

    VkExtent2D Swapchain::ChooseSwapExtent(SDL_Window* window)
    {
        // Surface capability data
        const auto& capabilities = swapChainInfo.capabilities;

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
}