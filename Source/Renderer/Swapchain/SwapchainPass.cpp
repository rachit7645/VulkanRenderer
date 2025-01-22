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
#include "Util/Ranges.h"
#include "Renderer/RenderConstants.h"

namespace Renderer::Swapchain
{
    SwapchainPass::SwapchainPass
    (
        Engine::Window& window,
        const Vk::Context& context,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager
    )
        : swapchain(window, context),
          pipeline(context, megaSet, textureManager, swapchain.imageFormat)
    {
        for (usize i = 0; i < cmdBuffers.size(); ++i)
        {
            cmdBuffers[i] = Vk::CommandBuffer
            (
                context.device,
                context.commandPool,
                VK_COMMAND_BUFFER_LEVEL_PRIMARY
            );

            #ifdef ENGINE_DEBUG
            auto name = fmt::format("SwapchainPass/FIF{}", i);

            VkDebugUtilsObjectNameInfoEXT nameInfo =
            {
                .sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .pNext        = nullptr,
                .objectType   = VK_OBJECT_TYPE_COMMAND_BUFFER,
                .objectHandle = std::bit_cast<u64>(cmdBuffers[i].handle),
                .pObjectName  = name.c_str()
            };

            vkSetDebugUtilsObjectNameEXT(context.device, &nameInfo);
            #endif
        }

        Logger::Info("{}\n", "Created swapchain pass!");
    }

    void SwapchainPass::Recreate
    (
        Engine::Window& window,
        const Vk::Context& context,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager
    )
    {
        swapchain.RecreateSwapChain(window, context);

        pipeline.Destroy(context.device);
        pipeline = Swapchain::SwapchainPipeline(context, megaSet, textureManager, swapchain.imageFormat);

        Logger::Info("{}\n", "Recreated swapchain pass!");
    }

    void SwapchainPass::Render(const Vk::MegaSet& megaSet, usize FIF)
    {
        auto& currentCmdBuffer = cmdBuffers[FIF];
        auto& currentImage     = swapchain.images[swapchain.imageIndex];
        auto& currentImageView = swapchain.imageViews[swapchain.imageIndex];

        currentCmdBuffer.Reset(0);
        currentCmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        #ifdef ENGINE_DEBUG
        auto name = fmt::format("SwapchainPass/{}", FIF);

        const VkDebugUtilsLabelEXT label =
        {
            .sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
            .pNext      = nullptr,
            .pLabelName = name.c_str(),
            .color      = {}
        };

        vkCmdBeginDebugUtilsLabelEXT(currentCmdBuffer.handle, &label);
        #endif

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

        pipeline.pushConstant =
        {
            .samplerIndex = pipeline.samplerIndex,
            .imageIndex   = pipeline.colorAttachmentIndex
        };

        pipeline.LoadPushConstants
        (
            currentCmdBuffer,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(Swapchain::PushConstant),
            reinterpret_cast<void*>(&pipeline.pushConstant)
        );

        // Mega set
        std::array descriptorSets = {megaSet.descriptorSet.handle};

        pipeline.BindDescriptors
        (
            currentCmdBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            0,
            descriptorSets
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

        #ifdef ENGINE_DEBUG
        vkCmdEndDebugUtilsLabelEXT(currentCmdBuffer.handle);
        #endif

        currentCmdBuffer.EndRecording();
    }

    void SwapchainPass::Present(VkQueue queue, usize FIF)
    {
        swapchain.Present(queue, FIF);
    }

    void SwapchainPass::Destroy(VkDevice device, VkCommandPool cmdPool)
    {
        Logger::Debug("{}\n", "Destroying swapchain pass!");

        for (auto&& cmdBuffer : cmdBuffers)
        {
            cmdBuffer.Free(device, cmdPool);
        }

        swapchain.Destroy(device);
        pipeline.Destroy(device);
    }
}