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
#include "Constants.h"
#include "Engine/Window.h"

namespace Vk
{
    class Swapchain
    {
    public:
        Swapchain(Engine::Window& window, const Vk::Context& context);
        void RecreateSwapChain(Engine::Window& window, const Vk::Context& context);
        void Destroy(VkDevice device);

        void Present(VkQueue queue, usize FIF);
        bool IsSwapchainValid();
        void AcquireSwapChainImage(VkDevice device, usize FIF);

        // Swap chain data
        VkSwapchainKHR handle = VK_NULL_HANDLE;
        VkExtent2D     extent = {};

        // Swapchain images data
        VkFormat                   imageFormat = {};
        std::vector<Vk::Image>     images      = {};
        std::vector<Vk::ImageView> imageViews  = {};
        u32                        imageIndex  = 0;

        // Semaphores
        std::array<VkSemaphore, FRAMES_IN_FLIGHT> imageAvailableSemaphores = {};
        std::array<VkSemaphore, FRAMES_IN_FLIGHT> renderFinishedSemaphores = {};
    private:
        void CreateSwapChain(const Engine::Window& window, const Vk::Context& context);
        void DestroySwapchain(VkDevice device);

        void CreateSyncObjects(VkDevice device);

        [[nodiscard]] VkSurfaceFormatKHR ChooseSurfaceFormat() const;
        [[nodiscard]] VkPresentModeKHR ChoosePresentationMode() const;
        [[nodiscard]] VkExtent2D ChooseSwapExtent(SDL_Window* window) const;

        // Swapchain info
        Vk::SwapchainInfo       m_swapChainInfo = {};
        std::array<VkResult, 2> m_status        = {VK_SUCCESS, VK_SUCCESS};
    };
}

#endif
