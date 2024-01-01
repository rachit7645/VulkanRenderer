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
#include "Vulkan/Builders/RenderPassBuilder.h"
#include "Vulkan/Builders/SubpassBuilder.h"
#include "Util/Log.h"
#include "Renderer/RenderConstants.h"

namespace Renderer::RenderPasses
{
    SwapPass::SwapPass(const std::shared_ptr<Engine::Window>& window, const std::shared_ptr<Vk::Context>& context)
        : swapchain(window, context)
    {
        // Initialise swapchain pass
        InitData(context);
        // Create command buffers
        CreateCmdBuffers(context);
        // Log
        Logger::Info("{}\n", "Created swapchain pass!");
    }

    void SwapPass::Recreate(const std::shared_ptr<Engine::Window>& window, const std::shared_ptr<Vk::Context>& context)
    {
        // Recreate swap chain
        swapchain.RecreateSwapChain(window, context);
        // Destroy data
        DestroyData(context->device, context->allocator);
        // Init swapchain data
        InitData(context);
        // Log
        Logger::Info("{}\n", "Recreated swapchain pass!");
    }

    void SwapPass::Render(usize FIF)
    {
        // Get current resources
        auto& currentCmdBuffer   = cmdBuffers[FIF];
        auto& currentFramebuffer = framebuffers[swapchain.imageIndex];

        // Begin recording
        currentCmdBuffer.Reset(0);
        currentCmdBuffer.BeginRecording(0);

        // Set clear state
        renderPass.ResetClearValues();
        renderPass.SetClearValue(Renderer::CLEAR_COLOR);
        // Begin render pass
        renderPass.BeginRenderPass
        (
            currentCmdBuffer,
            currentFramebuffer,
            {.offset = {0, 0}, .extent = swapchain.extent},
            VK_SUBPASS_CONTENTS_INLINE
        );

        // Bind pipeline
        pipeline.Bind(currentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);

        // Viewport
        VkViewport viewport =
        {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<f32>(currentFramebuffer.size.x),
            .height   = static_cast<f32>(currentFramebuffer.size.y),
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
            .extent = {currentFramebuffer.size.x, currentFramebuffer.size.y}
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

        // End render pass
        renderPass.EndRenderPass(currentCmdBuffer);
        // End recording
        currentCmdBuffer.EndRecording();
    }

    void SwapPass::Present(const std::shared_ptr<Vk::Context>& context, usize FIF)
    {
        // Present
        swapchain.Present(context->graphicsQueue, FIF);
    }

    void SwapPass::InitData(const std::shared_ptr<Vk::Context>& context)
    {
        // Create render pass
        CreateRenderPass(context->device);
        // Create pipeline
        pipeline = Pipelines::SwapPipeline(context, renderPass, swapchain.extent);
        // Create framebuffers
        CreateFramebuffers(context->device);
    }

    void SwapPass::CreateRenderPass(VkDevice device)
    {
        // Build render pass
        renderPass = Vk::Builders::RenderPassBuilder(device)
                    .AddAttachment(
                        swapchain.imageFormat,
                        VK_SAMPLE_COUNT_1_BIT,
                        VK_ATTACHMENT_LOAD_OP_CLEAR,
                        VK_ATTACHMENT_STORE_OP_STORE,
                        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                        VK_ATTACHMENT_STORE_OP_DONT_CARE,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
                    .AddSubpass(
                        Vk::Builders::SubpassBuilder()
                        .AddColorReference(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                        .SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS)
                        .AddDependency(
                            VK_SUBPASS_EXTERNAL,
                            0,
                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                            0,
                            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
                        .Build())
                    .Build();
    }

    void SwapPass::CreateFramebuffers(VkDevice device)
    {
        // Resize
        framebuffers.reserve(swapchain.imageViews.size());

        // For each image view
        for (auto& imageView : swapchain.imageViews)
        {
            // Attachments
            std::array<Vk::ImageView, 1> attachmentViews = { imageView };
            // Create framebuffer
            framebuffers.emplace_back
            (
                device,
                renderPass,
                attachmentViews,
                glm::uvec2(swapchain.extent.width, swapchain.extent.height),
                1
            );
        }
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

        // Destroy data
        DestroyData(device, allocator);
        // Destroy swapchain
        swapchain.Destroy(device);
    }

    void SwapPass::DestroyData(VkDevice device, VmaAllocator allocator)
    {
        // Destroy renderpass
        renderPass.Destroy(device);
        // Destroy pipeline
        pipeline.Destroy(device, allocator);

        // Destroy framebuffers
        for (auto&& framebuffer : framebuffers)
        {
            framebuffer.Destroy(device);
        }

        // Clear framebuffers
        framebuffers.clear();
    }
}