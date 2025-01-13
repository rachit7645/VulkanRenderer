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

#include "SwapchainPass.h"

#include "Util/Log.h"
#include "Renderer/RenderConstants.h"

namespace Renderer::Swapchain
{
    SwapchainPass::SwapchainPass(Engine::Window& window, Vk::Context& context)
        : swapchain(window, context),
          pipeline(context, swapchain.imageFormat)
    {
        CreateCmdBuffers(context);
        Logger::Info("{}\n", "Created swapchain pass!");
    }

    void SwapchainPass::Recreate(Engine::Window& window, Vk::Context& context)
    {
        swapchain.RecreateSwapChain(window, context);

        pipeline.Destroy(context.device);
        pipeline = Swapchain::SwapchainPipeline(context, swapchain.imageFormat);

        Logger::Info("{}\n", "Recreated swapchain pass!");
    }

    void SwapchainPass::Render(Vk::DescriptorCache& descriptorCache, usize FIF)
    {
        auto& currentCmdBuffer = cmdBuffers[FIF];
        auto& currentImage     = swapchain.images[swapchain.imageIndex];
        auto& currentImageView = swapchain.imageViews[swapchain.imageIndex];

        currentCmdBuffer.Reset(0);
        currentCmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        currentImage.Barrier
        (
            currentCmdBuffer,
            VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
            VK_ACCESS_2_NONE,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            {
                .aspectMask     = currentImage.aspect,
                .baseMipLevel   = 0,
                .levelCount     = currentImage.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        VkRenderingAttachmentInfo colorAttachmentInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = currentImageView.handle,
            .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode        = VK_RESOLVE_MODE_NONE,
            .resolveImageView   = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp             = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
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

        vkCmdBeginRendering(currentCmdBuffer.handle, &renderInfo);

        pipeline.Bind(currentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);

        VkViewport viewport =
        {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<f32>(swapchain.extent.width),
            .height   = static_cast<f32>(swapchain.extent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vkCmdSetViewportWithCount(currentCmdBuffer.handle, 1, &viewport);

        VkRect2D scissor =
        {
            .offset = {0, 0},
            .extent = swapchain.extent
        };

        vkCmdSetScissorWithCount(currentCmdBuffer.handle, 1, &scissor);

        pipeline.BindDescriptors
        (
            currentCmdBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            0,
            std::span(&pipeline.GetImageSets(descriptorCache)[FIF].handle, 1)
        );

        vkCmdDraw
        (
            currentCmdBuffer.handle,
            3,
            1,
            0,
            0
        );

        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), currentCmdBuffer.handle);

        vkCmdEndRendering(currentCmdBuffer.handle);

        currentImage.Barrier
        (
            currentCmdBuffer,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_NONE,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            {
                .aspectMask     = currentImage.aspect,
                .baseMipLevel   = 0,
                .levelCount     = currentImage.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        currentCmdBuffer.EndRecording();
    }

    void SwapchainPass::Present(VkQueue queue, usize FIF)
    {
        swapchain.Present(queue, FIF);
    }

    void SwapchainPass::CreateCmdBuffers(const Vk::Context& context)
    {
        for (usize i = 0; i < cmdBuffers.size(); ++i)
        {
            cmdBuffers[i] = Vk::CommandBuffer
            (
                context,
                VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                "SwapchainPass/FIF" + std::to_string(i)
            );
        }
    }

    void SwapchainPass::Destroy(const Vk::Context& context)
    {
        Logger::Debug("{}\n", "Destroying swapchain pass!");

        for (auto&& cmdBuffer : cmdBuffers)
        {
            cmdBuffer.Free(context);
        }

        swapchain.Destroy(context.device);
        pipeline.Destroy(context.device);
    }
}