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

#include "RenderPass.h"

#include "Util/Log.h"
#include "Util/Ranges.h"
#include "Renderer/RenderConstants.h"
#include "Renderer/Depth/RenderPass.h"
#include "Vulkan/DebugUtils.h"

namespace Renderer::TAA
{
    constexpr usize TAA_HISTORY_SIZE = 2;

    RenderPass::RenderPass
    (
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        Vk::FramebufferManager& framebufferManager,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager
    )
        : pipeline(context, formatHelper, megaSet, textureManager)
    {
        for (usize i = 0; i < cmdBuffers.size(); ++i)
        {
            cmdBuffers[i] = Vk::CommandBuffer
            (
                context.device,
                context.commandPool,
                VK_COMMAND_BUFFER_LEVEL_PRIMARY
            );

            Vk::SetDebugName(context.device, cmdBuffers[i].handle, fmt::format("TAAPass/FIF{}", i));
        }

        framebufferManager.AddFramebuffer
        (
            "ResolvedSceneColor",
            Vk::FramebufferType::ColorHDR,
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferUsage::Sampled,
            [] (const VkExtent2D& extent, UNUSED Vk::FramebufferManager& framebufferManager) -> Vk::FramebufferSize
            {
                return
                {
                    .width       = extent.width,
                    .height      = extent.height,
                    .mipLevels   = 1,
                    .arrayLayers = 1
                };
            }
        );

        framebufferManager.AddFramebuffer
        (
            "TAABuffer",
            Vk::FramebufferType::ColorHDR_WithAlpha,
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferUsage::Sampled | Vk::FramebufferUsage::TransferDestination,
            [] (const VkExtent2D& extent, UNUSED Vk::FramebufferManager& framebufferManager) -> Vk::FramebufferSize
            {
                return
                {
                    .width       = extent.width,
                    .height      = extent.height,
                    .mipLevels   = 1,
                    .arrayLayers = TAA_HISTORY_SIZE
                };
            }
        );

        framebufferManager.AddFramebufferView
        (
            "ResolvedSceneColor",
            "ResolvedSceneColorView",
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        for (usize i = 0; i < TAA_HISTORY_SIZE; ++i)
        {
            framebufferManager.AddFramebufferView
            (
                "TAABuffer",
                fmt::format("TAABufferView/{}", i),
                Vk::FramebufferImageType::Single2D,
                Vk::FramebufferViewSize{
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = static_cast<u32>(i),
                    .layerCount     = 1
                }
            );
        }

        Logger::Info("{}\n", "Created TAA pass!");
    }

    void RenderPass::Render
    (
        usize FIF,
        usize frameIndex,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet
    )
    {
        const auto& currentCmdBuffer = cmdBuffers[FIF];

        currentCmdBuffer.Reset(0);
        currentCmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        Vk::BeginLabel(currentCmdBuffer, fmt::format("TAAPass/FIF{}", FIF), glm::vec4(0.6098f, 0.7843f, 0.7549f, 1.0f));

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("TAA"))
            {
                if (ImGui::Button("Reset History"))
                {
                    m_hasToResetHistory = true;
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        if (m_hasToResetHistory)
        {
            const auto& history = framebufferManager.GetFramebuffer("TAABuffer");

            history.image.Barrier
            (
                currentCmdBuffer,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                VK_PIPELINE_STAGE_2_CLEAR_BIT,
                VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                {
                    .aspectMask     = history.image.aspect,
                    .baseMipLevel   = 0,
                    .levelCount     = history.image.mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount     = history.image.arrayLayers
                }
            );

            constexpr VkClearColorValue clearColor = {.float32 = {0.0f, 0.0f, 0.0f, 1.0f}};

            const VkImageSubresourceRange subresourceRange =
            {
                .aspectMask     = history.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = history.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = history.image.arrayLayers
            };

            vkCmdClearColorImage
            (
                currentCmdBuffer.handle,
                history.image.handle,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                &clearColor,
                1,
                &subresourceRange
            );

            history.image.Barrier
            (
                currentCmdBuffer,
                VK_PIPELINE_STAGE_2_CLEAR_BIT,
                VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                {
                    .aspectMask     = history.image.aspect,
                    .baseMipLevel   = 0,
                    .levelCount     = history.image.mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount     = history.image.arrayLayers
                }
            );

            m_hasToResetHistory = false;
        }

        const usize currentIndex  = frameIndex                          % TAA_HISTORY_SIZE;
        const usize previousIndex = (frameIndex + TAA_HISTORY_SIZE - 1) % TAA_HISTORY_SIZE;

        const auto& resolvedView = framebufferManager.GetFramebufferView("ResolvedSceneColorView");
        const auto& historyView  = framebufferManager.GetFramebufferView(fmt::format("TAABufferView/{}", currentIndex));

        const auto& resolved = framebufferManager.GetFramebuffer(resolvedView.framebuffer);
        const auto& history  = framebufferManager.GetFramebuffer(historyView.framebuffer);

        resolved.image.Barrier
        (
            currentCmdBuffer,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            {
                .aspectMask     = resolved.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = resolved.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = resolved.image.arrayLayers
            }
        );

        history.image.Barrier
        (
            currentCmdBuffer,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            {
                .aspectMask     = history.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = history.image.mipLevels,
                .baseArrayLayer = static_cast<u32>(currentIndex),
                .layerCount     = 1
            }
        );

        const VkRenderingAttachmentInfo resolvedInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = resolvedView.view.handle,
            .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode        = VK_RESOLVE_MODE_NONE,
            .resolveImageView   = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp             = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue         = {}
        };

        const VkRenderingAttachmentInfo historyInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = historyView.view.handle,
            .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode        = VK_RESOLVE_MODE_NONE,
            .resolveImageView   = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp             = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue         = {}
        };

        const std::array colorAttachments = {resolvedInfo, historyInfo};

        const VkRenderingInfo renderInfo =
        {
            .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext                = nullptr,
            .flags                = 0,
            .renderArea           = {
                .offset = {0, 0},
                .extent = {resolved.image.width, resolved.image.height}
            },
            .layerCount           = 1,
            .viewMask             = 0,
            .colorAttachmentCount = static_cast<u32>(colorAttachments.size()),
            .pColorAttachments    = colorAttachments.data(),
            .pDepthAttachment     = nullptr,
            .pStencilAttachment   = nullptr
        };

        vkCmdBeginRendering(currentCmdBuffer.handle, &renderInfo);

        pipeline.Bind(currentCmdBuffer);

        const VkViewport viewport =
        {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<f32>(resolved.image.width),
            .height   = static_cast<f32>(resolved.image.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vkCmdSetViewportWithCount(currentCmdBuffer.handle, 1, &viewport);

        const VkRect2D scissor =
        {
            .offset = {0, 0},
            .extent = {resolved.image.width, resolved.image.height}
        };

        vkCmdSetScissorWithCount(currentCmdBuffer.handle, 1, &scissor);

        pipeline.pushConstant =
        {
            .pointSamplerIndex  = pipeline.pointSamplerIndex,
            .linearSamplerIndex = pipeline.linearSamplerIndex,
            .currentColorIndex  = framebufferManager.GetFramebufferView("SceneColorView").sampledImageIndex,
            .historyBufferIndex = framebufferManager.GetFramebufferView(fmt::format("TAABufferView/{}", previousIndex)).sampledImageIndex,
            .velocityIndex      = framebufferManager.GetFramebufferView("MotionVectorsView").sampledImageIndex,
            .sceneDepthIndex    = framebufferManager.GetFramebufferView(fmt::format("SceneDepthView/{}", frameIndex % Depth::DEPTH_HISTORY_SIZE)).sampledImageIndex
        };

        pipeline.PushConstants
        (
            currentCmdBuffer,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(TAA::PushConstant),
            &pipeline.pushConstant
        );

        const std::array descriptorSets = {megaSet.descriptorSet};
        pipeline.BindDescriptors(currentCmdBuffer, 0, descriptorSets);

        vkCmdDraw
        (
            currentCmdBuffer.handle,
            3,
            1,
            0,
            0
        );

        vkCmdEndRendering(currentCmdBuffer.handle);

        resolved.image.Barrier
        (
            currentCmdBuffer,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            {
                .aspectMask     = resolved.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = resolved.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = resolved.image.arrayLayers
            }
        );

        history.image.Barrier
        (
            currentCmdBuffer,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            {
                .aspectMask     = history.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = history.image.mipLevels,
                .baseArrayLayer = static_cast<u32>(currentIndex),
                .layerCount     = 1
            }
        );

        Vk::EndLabel(currentCmdBuffer);

        currentCmdBuffer.EndRecording();
    }

    void RenderPass::ResetHistory()
    {
        m_hasToResetHistory = true;
    }

    void RenderPass::Destroy(VkDevice device, VkCommandPool cmdPool)
    {
        Logger::Debug("{}\n", "Destroying TAA pass!");

        for (auto&& cmdBuffer : cmdBuffers)
        {
            cmdBuffer.Free(device, cmdPool);
        }

        pipeline.Destroy(device);
    }
}
