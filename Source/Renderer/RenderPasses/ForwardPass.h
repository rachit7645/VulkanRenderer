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

#ifndef FORWARD_PASS_H
#define FORWARD_PASS_H

#include "Vulkan/CommandBuffer.h"
#include "Vulkan/RenderPass.h"
#include "Renderer/FreeCamera.h"
#include "Renderer/Pipelines/ForwardPipeline.h"
#include "Vulkan/DepthBuffer.h"
#include "Models/Model.h"

namespace Renderer::RenderPasses
{
    class ForwardPass
    {
    public:
        // Create forward pass
        ForwardPass(const std::shared_ptr<Vk::Context>& context, VkExtent2D swapchainExtent);
        // Recreate forward pass data
        void Recreate(const std::shared_ptr<Vk::Context>& context, VkExtent2D swapchainExtent);
        // Destroy
        void Destroy(VkDevice device);

        // Render
        void Render(usize FIF, const Renderer::FreeCamera& camera, const Models::Model& model);

        // Forward render pass
        Vk::RenderPass renderPass;
        // Pipeline
        Pipelines::ForwardPipeline pipeline;
        // Command buffers
        std::array<Vk::CommandBuffer, Vk::FRAMES_IN_FLIGHT> cmdBuffers;

        // Images
        std::array<Vk::Image, Vk::FRAMES_IN_FLIGHT> images;
        // Image views
        std::array<Vk::ImageView, Vk::FRAMES_IN_FLIGHT> imageViews;
        // Depth buffer
        Vk::DepthBuffer depthBuffer;
        // Framebuffers
        std::array<Vk::Framebuffer, Vk::FRAMES_IN_FLIGHT> framebuffers;
    private:
        // Init
        void InitData(const std::shared_ptr<Vk::Context>& context, VkExtent2D swapchainExtent);
        // Destroy data
        void DestroyData(VkDevice device);

        // Init framebuffers
        void InitFramebuffers(const std::shared_ptr<Vk::Context>& context, VkExtent2D swapchainExtent);
        // Create render pass
        void CreateRenderPass(VkDevice device, VkPhysicalDevice physicalDevice);

        // Get color format
        VkFormat GetColorFormat(VkPhysicalDevice physicalDevice);
    };
}

#endif