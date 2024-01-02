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

#include "SwapPass.h"
#include "Util/Log.h"
#include "Renderer/RenderConstants.h"

namespace Renderer::RenderPasses
{
    SwapPass::SwapPass(const std::shared_ptr<Engine::Window>& window, const std::shared_ptr<Vk::Context>& context)
        : swapchain(window, context),
          pipeline(context, swapchain.imageFormat, swapchain.extent)
    {
        // Create command buffers
        CreateCmdBuffers(context);
        // Log
        Logger::Info("{}\n", "Created swapchain pass!");
    }

    void SwapPass::Recreate(const std::shared_ptr<Engine::Window>& window, const std::shared_ptr<Vk::Context>& context)
    {
        // Recreate swap chain
        swapchain.RecreateSwapChain(window, context);
        // Recreate pipeline just in case
        pipeline = Pipelines::SwapPipeline(context, swapchain.imageFormat, swapchain.extent);
        // Log
        Logger::Info("{}\n", "Recreated swapchain pass!");
    }

    void SwapPass::Render(usize FIF)
    {
        // Get current resources
        auto& currentCmdBuffer = cmdBuffers[FIF];
        auto& currentImage     = swapchain.images[swapchain.imageIndex];
        auto& currentImageView = swapchain.imageViews[swapchain.imageIndex];

        // Begin recording
        currentCmdBuffer.Reset(0);
        currentCmdBuffer.BeginRecording(0);

        // Transition as color attachment
        currentImage.TransitionLayout
        (
            currentCmdBuffer,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        );

        // Color attachment info
        VkRenderingAttachmentInfo colorAttachmentInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = currentImageView.handle,
            .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode        = VK_RESOLVE_MODE_NONE,
            .resolveImageView   = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue         = {{{
                Renderer::CLEAR_COLOR.r,
                Renderer::CLEAR_COLOR.g,
                Renderer::CLEAR_COLOR.b,
                Renderer::CLEAR_COLOR.a
            }}}
        };

        VkRenderingInfo renderInfo =
        {
            .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext                = nullptr,
            .flags                = 0,
            .renderArea           = {
                .offset = {0, 0},
                .extent = swapchain.extent
            },
            .layerCount           = 1,
            .viewMask             = 0,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &colorAttachmentInfo,
            .pDepthAttachment     = nullptr,
            .pStencilAttachment   = nullptr
        };

        // Begin rendering
        vkCmdBeginRendering(currentCmdBuffer.handle, &renderInfo);

        // Bind pipeline
        pipeline.Bind(currentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);

        // Viewport
        VkViewport viewport =
        {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<f32>(swapchain.extent.width),
            .height   = static_cast<f32>(swapchain.extent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        // Set viewport
        vkCmdSetViewport
        (
            currentCmdBuffer.handle,
            0,
            1,
            &viewport
        );

        // Scissor
        VkRect2D scissor =
        {
            .offset = {0, 0},
            .extent = swapchain.extent
        };

        // Set scissor
        vkCmdSetScissor
        (
            currentCmdBuffer.handle,
            0,
            1,
            &scissor
        );

        // Load image descriptor
        pipeline.BindDescriptors
        (
            currentCmdBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            0,
            std::span(pipeline.GetImageData().setMap[FIF].data(), 1)
        );

        // Bind vertex buffer
        pipeline.screenQuad.Bind(currentCmdBuffer);
        // Draw quad
        vkCmdDraw
        (
            currentCmdBuffer.handle,
            pipeline.screenQuad.vertexCount,
            1,
            0,
            0
        );

        // Render ImGui
        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), currentCmdBuffer.handle);

        // End render
        vkCmdEndRendering(currentCmdBuffer.handle);

        // Transition for presentation
        currentImage.TransitionLayout
        (
            currentCmdBuffer,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        );

        // End recording
        currentCmdBuffer.EndRecording();
    }

    void SwapPass::Present(const std::shared_ptr<Vk::Context>& context, usize FIF)
    {
        // Present
        swapchain.Present(context->graphicsQueue, FIF);
    }

    void SwapPass::CreateCmdBuffers(const std::shared_ptr<Vk::Context>& context)
    {
        // Create
        for (auto&& cmdBuffer : cmdBuffers)
        {
            cmdBuffer = Vk::CommandBuffer(context, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        }
    }

    void SwapPass::Destroy(VkDevice device, VmaAllocator allocator)
    {
        // Log
        Logger::Debug("{}\n", "Destroying swapchain pass!");

        // Destroy swapchain
        swapchain.Destroy(device);
        // Destroy pipeline
        pipeline.Destroy(device, allocator);
    }
}