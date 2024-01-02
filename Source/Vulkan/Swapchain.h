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

namespace Vk
{
    class Swapchain
    {
    public:
        // Create swapchain
        Swapchain(const std::shared_ptr<Engine::Window>& window, const std::shared_ptr<Vk::Context>& context);
        // Recreate swap chain
        void RecreateSwapChain(const std::shared_ptr<Engine::Window>& window, const std::shared_ptr<Vk::Context>& context);
        // Destroys everything
        void Destroy(VkDevice device);

        // Present
        void Present(VkQueue queue, usize FIF);
        // Check if swap chain is valid
        bool IsSwapchainValid();
        // Acquire swap chain image
        void AcquireSwapChainImage(VkDevice device, usize FIF);

        // Swap chain
        VkSwapchainKHR handle = {};
        // Extent
        VkExtent2D extent = {};

        // Image format
        VkFormat imageFormat = {};
        // Swap chain images
        std::vector<Vk::Image> images = {};
        // Swap chain image views
        std::vector<Vk::ImageView> imageViews = {};
        // Image index
        u32 imageIndex = 0;

        // Semaphores
        std::array<VkSemaphore, FRAMES_IN_FLIGHT> imageAvailableSemaphores = {};
        std::array<VkSemaphore, FRAMES_IN_FLIGHT> renderFinishedSemaphores = {};
    private:
        // Create swap chain
        void CreateSwapChain(const std::shared_ptr<Engine::Window>& window, const std::shared_ptr<Vk::Context>& context);
        // Destroy swapchain data
        void DestroySwapchain(VkDevice device);
        // Create image views
        void CreateImageViews(VkDevice device);
        // Create sync objects
        void CreateSyncObjects(VkDevice device);

        // Choose surface format
        [[nodiscard]] VkSurfaceFormatKHR ChooseSurfaceFormat() const;
        // Choose surface presentation mode
        [[nodiscard]] VkPresentModeKHR ChoosePresentationMode() const;
        // Choose swap extent
        [[nodiscard]] VkExtent2D ChooseSwapExtent(SDL_Window* window) const;

        // Swapchain info
        Vk::SwapchainInfo m_swapChainInfo = {};
        // Status
        std::array<VkResult, 2> m_status = {VK_SUCCESS, VK_SUCCESS};
    };
}

#endif
