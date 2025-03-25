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

namespace Vk
{
    class Swapchain
    {
    public:
        Swapchain(const glm::ivec2& size, const Vk::Context& context);

        bool IsSurfaceValid(const glm::ivec2& size, const Vk::Context& context);
        void RecreateSwapChain(const Vk::Context& context);

        void Destroy(VkDevice device);

        VkResult Present(VkDevice device, VkQueue queue);
        VkResult AcquireSwapChainImage(VkDevice device, usize FIF);

        VkSwapchainKHR handle = VK_NULL_HANDLE;
        VkExtent2D     extent = {};

        VkFormat                   imageFormat = {};
        std::vector<Vk::Image>     images      = {};
        std::vector<Vk::ImageView> imageViews  = {};
        u32                        imageIndex  = 0;

        std::array<VkSemaphore, FRAMES_IN_FLIGHT> imageAvailableSemaphores = {};

        std::vector<VkSemaphore> renderFinishedSemaphores = {};
        std::vector<VkFence>     presentFences            = {};
    private:
        void CreateSwapChain(const Vk::Context& context);
        void DestroySwapchain(VkDevice device);

        void CreateStaticSyncObjects(VkDevice device);
        void CreateSyncObjects(VkDevice device);

        [[nodiscard]] VkSurfaceFormat2KHR ChooseSurfaceFormat() const;
        [[nodiscard]] VkPresentModeKHR ChoosePresentationMode() const;
        [[nodiscard]] VkExtent2D ChooseSwapExtent(const glm::ivec2& size) const;

        Vk::SwapchainInfo m_swapChainInfo = {};
    };
}

#endif
