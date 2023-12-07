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

#ifndef VK_SWAPCHAIN_H
#define VK_SWAPCHAIN_H

#include <vector>
#include <memory>

#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>

#include "SwapchainInfo.h"
#include "Image.h"
#include "ImageView.h"
#include "Context.h"
#include "Engine/Window.h"
#include "DepthBuffer.h"
#include "Framebuffer.h"

namespace Vk
{
    class Swapchain
    {
    public:
        // Default constructor
        Swapchain() = default;
        // Create swapchain
        Swapchain(const std::shared_ptr<Engine::Window>& window, const std::shared_ptr<Vk::Context>& context);

        // Recreate swap chain
        void RecreateSwapChain(const std::shared_ptr<Engine::Window>& window, const std::shared_ptr<Vk::Context>& context);
        // Destroys everything
        void Destroy(VkDevice device);

        // Swap chain
        VkSwapchainKHR handle = {};
        // Extent
        VkExtent2D extent = {};
        // Swap chain framebuffers
        std::vector<Vk::Framebuffer> framebuffers = {};
        // Swap chain depth buffer
        Vk::DepthBuffer depthBuffer;
        // Swap chain presentation render pass
        VkRenderPass renderPass = {};
        // Swapchain info
        Vk::SwapchainInfo swapChainInfo = {};
    private:
        // Destroy current swap chain
        void DestroySwapChain(VkDevice device);
        // Create swap chain
        void CreateSwapChain(const std::shared_ptr<Engine::Window>& window, const std::shared_ptr<Vk::Context>& context);
        // Create image views
        void CreateImageViews(VkDevice device);
        // Create depth buffer
        void CreateDepthBuffer(const std::shared_ptr<Vk::Context>& context);
        // Creates the default render pass
        void CreateRenderPass(VkDevice device);
        // Create swap chain framebuffers
        void CreateFramebuffers(VkDevice device);

        // Choose surface format
        [[nodiscard]] VkSurfaceFormatKHR ChooseSurfaceFormat();
        // Choose surface presentation mode
        [[nodiscard]] VkPresentModeKHR ChoosePresentationMode();
        // Choose swap extent
        [[nodiscard]] VkExtent2D ChooseSwapExtent(SDL_Window* window);

        // Swap chain images
        std::vector<Vk::Image> m_images = {};
        // Image format
        VkFormat m_imageFormat = {};
        // Swap chain image views
        std::vector<Vk::ImageView> m_imageViews = {};
    };
}

#endif
