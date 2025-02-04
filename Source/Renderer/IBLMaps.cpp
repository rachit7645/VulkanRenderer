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

#include "IBLMaps.h"

#include "Util/Log.h"
#include "Renderer/RenderConstants.h"
#include "Vulkan/DebugUtils.h"

namespace Renderer
{
    constexpr VkExtent2D BRDF_LUT_SIZE = {1024, 1024};

    IBLMaps::IBLMaps(Vk::FramebufferManager& framebufferManager)
    {
        framebufferManager.AddFramebuffer(Vk::FramebufferManager::FramebufferType::BRDF, "BRDF_LUT", BRDF_LUT_SIZE);
    }

    void IBLMaps::Generate
    (
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        const Vk::FramebufferManager& framebufferManager
    )
    {
        auto brdfPipeline = BRDF::Pipeline(context, formatHelper);

        auto cmdBuffer = Vk::CommandBuffer
        (
            context.device,
            context.commandPool,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY
        );

        Vk::BeginLabel(context.graphicsQueue, "IBLMaps::Generate", {0.9215f, 0.8470f, 0.0274f, 1.0f});

        // Record
        {
            cmdBuffer.Reset(0);
            cmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

            CreateBRDFLUT(cmdBuffer, brdfPipeline, framebufferManager);

            cmdBuffer.EndRecording();
        }

        VkFence renderFence = VK_NULL_HANDLE;

        // Submit
        {
            const VkFenceCreateInfo fenceCreateInfo =
            {
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0
            };

            vkCreateFence(context.device, &fenceCreateInfo, nullptr, &renderFence);

            VkCommandBufferSubmitInfo cmdBufferInfo =
            {
                .sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .pNext         = nullptr,
                .commandBuffer = cmdBuffer.handle,
                .deviceMask    = 0
            };

            const VkSubmitInfo2 submitInfo =
            {
                .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .pNext                    = nullptr,
                .flags                    = 0,
                .waitSemaphoreInfoCount   = 0,
                .pWaitSemaphoreInfos      = nullptr,
                .commandBufferInfoCount   = 1,
                .pCommandBufferInfos      = &cmdBufferInfo,
                .signalSemaphoreInfoCount = 0,
                .pSignalSemaphoreInfos    = nullptr
            };

            Vk::CheckResult(vkQueueSubmit2(
                context.graphicsQueue,
                1,
                &submitInfo,
                renderFence),
                "Failed to submit IBL command buffer!"
            );

            Vk::CheckResult(vkWaitForFences(
                context.device,
                1,
                &renderFence,
                VK_TRUE,
                std::numeric_limits<u64>::max()),
                "Error while waiting for IBL generation!"
            );
        }

        Vk::EndLabel(context.graphicsQueue);

        // Clean
        {
            vkDestroyFence(context.device, renderFence, nullptr);
            cmdBuffer.Free(context.device, context.commandPool);
            brdfPipeline.Destroy(context.device);
        }
    }

    void IBLMaps::CreateBRDFLUT
    (
        const Vk::CommandBuffer& cmdBuffer,
        const BRDF::Pipeline& pipeline,
        const Vk::FramebufferManager& framebufferManager
    )
    {
        const auto& brdfLut = framebufferManager.GetFramebuffer("BRDF_LUT");

        Vk::BeginLabel(cmdBuffer, "BRDF LUT Generation", {0.9215f, 0.0274f, 0.8588f, 1.0f});

        const VkRenderingAttachmentInfo colorAttachmentInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = brdfLut.imageView.handle,
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

        const VkRenderingInfo renderInfo =
        {
            .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext                = nullptr,
            .flags                = 0,
            .renderArea           = {
                .offset = {0, 0},
                .extent = {brdfLut.image.width, brdfLut.image.height}
            },
            .layerCount           = 1,
            .viewMask             = 0,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &colorAttachmentInfo,
            .pDepthAttachment     = nullptr,
            .pStencilAttachment   = nullptr
        };

        vkCmdBeginRendering(cmdBuffer.handle, &renderInfo);

        pipeline.Bind(cmdBuffer);

        const VkViewport viewport =
        {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<f32>(brdfLut.image.width),
            .height   = static_cast<f32>(brdfLut.image.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vkCmdSetViewportWithCount(cmdBuffer.handle, 1, &viewport);

        const VkRect2D scissor =
        {
            .offset = {0, 0},
            .extent = {brdfLut.image.width, brdfLut.image.height}
        };

        vkCmdSetScissorWithCount(cmdBuffer.handle, 1, &scissor);

        vkCmdDraw
        (
            cmdBuffer.handle,
            3,
            1,
            0,
            0
        );

        vkCmdEndRendering(cmdBuffer.handle);

        brdfLut.image.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            {
                .aspectMask     = brdfLut.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = brdfLut.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        Vk::EndLabel(cmdBuffer);
    }
}
