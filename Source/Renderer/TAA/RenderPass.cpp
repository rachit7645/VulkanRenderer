/*
 * Copyright (c) 2023 - 2026 Rachit
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
#include "Renderer/Depth/RenderPass.h"
#include "TAA/TAA.h"
#include "Vulkan/DebugUtils.h"
#include "Externals/ImGui.h"

namespace Renderer::TAA
{
    constexpr usize TAA_HISTORY_SIZE = 2;

    RenderPass::RenderPass
    (
        const Vk::FormatHelper& formatHelper,
        const Vk::MegaSet& megaSet,
        Vk::PipelineManager& pipelineManager,
        Vk::FramebufferManager& framebufferManager
    )
    {
        constexpr std::array DYNAMIC_STATES = {VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT, VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT};

        const std::array colorFormats = {formatHelper.colorAttachmentFormatHDR, formatHelper.colorAttachmentFormatHDR};

        pipelineManager.AddPipeline("TAA", Vk::PipelineConfig{}
            .SetPipelineType(VK_PIPELINE_BIND_POINT_GRAPHICS)
            .SetRenderingInfo(0, colorFormats, VK_FORMAT_UNDEFINED)
            .AttachShader("Misc/Triangle.vert", VK_SHADER_STAGE_VERTEX_BIT)
            .AttachShader("TAA/TAA.frag",       VK_SHADER_STAGE_FRAGMENT_BIT)
            .SetDynamicStates(DYNAMIC_STATES)
            .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetRasterizerState(VK_FALSE, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_FILL)
            .AddDefaultBlendAttachment()
            .AddDefaultBlendAttachment()
            .AddPushConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(TAA::Constants))
            .AddDescriptorLayout(megaSet.descriptorLayout)
        );

        framebufferManager.AddFramebuffer
        (
            "ResolvedSceneColor",
            Vk::FramebufferCustomFormat::ColorHDR,
            VK_IMAGE_VIEW_TYPE_2D,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            [] (ENGINE_UNUSED const VkExtent2D& renderExtent, const VkExtent2D& displayExtent) -> Vk::FramebufferSize
            {
                return
                {
                    .width       = displayExtent.width,
                    .height      = displayExtent.height,
                    .mipLevels   = 1,
                    .arrayLayers = 1
                };
            },
            Vk::FramebufferInitialState{
                .stageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .accessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .layout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        );

        framebufferManager.AddFramebuffer
        (
            "TAABuffer",
            Vk::FramebufferCustomFormat::ColorHDR,
            VK_IMAGE_VIEW_TYPE_2D,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            [] (ENGINE_UNUSED const VkExtent2D& renderExtent, const VkExtent2D& displayExtent) -> Vk::FramebufferSize
            {
                return
                {
                    .width       = displayExtent.width,
                    .height      = displayExtent.height,
                    .mipLevels   = 1,
                    .arrayLayers = TAA_HISTORY_SIZE
                };
            },
            Vk::FramebufferInitialState{
                .stageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .accessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .layout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        );

        framebufferManager.AddFramebufferView
        (
            "ResolvedSceneColor",
            "ResolvedSceneColorView",
            VK_IMAGE_VIEW_TYPE_2D,
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
                VK_IMAGE_VIEW_TYPE_2D,
                Vk::FramebufferViewSize{
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = static_cast<u32>(i),
                    .layerCount     = 1
                }
            );
        }
    }

    void RenderPass::Render
    (
        usize frameIndex,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Vk::TextureManager& textureManager,
        const Objects::GlobalSamplers& samplers
    )
    {
        Vk::BeginLabel(cmdBuffer, "TAA", glm::vec4(0.6098f, 0.7843f, 0.7549f, 1.0f));

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Effects"))
            {
                if (ImGui::CollapsingHeader("TAA"))
                {
                    if (ImGui::Button("Reset History"))
                    {
                        m_hasToResetHistory = true;
                    }
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
                cmdBuffer,
                Vk::ImageBarrier{
                    .srcStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                    .dstStageMask   = VK_PIPELINE_STAGE_2_CLEAR_BIT,
                    .dstAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .newLayout      = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .baseMipLevel   = 0,
                    .levelCount     = history.image.mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount     = history.image.arrayLayers
                }
            );

            constexpr VkClearColorValue BLACK = {.float32 = {0.0f, 0.0f, 0.0f, 1.0f}};

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
                cmdBuffer.handle,
                history.image.handle,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                &BLACK,
                1,
                &subresourceRange
            );

            history.image.Barrier
            (
                cmdBuffer,
                Vk::ImageBarrier{
                    .srcStageMask   = VK_PIPELINE_STAGE_2_CLEAR_BIT,
                    .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                    .oldLayout      = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .baseMipLevel   = 0,
                    .levelCount     = history.image.mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount     = history.image.arrayLayers
                }
            );

            m_hasToResetHistory = false;
        }

        const auto& pipeline = pipelineManager.GetPipeline("TAA");

        const usize currentIndex  = frameIndex                          % TAA_HISTORY_SIZE;
        const usize previousIndex = (frameIndex + TAA_HISTORY_SIZE - 1) % TAA_HISTORY_SIZE;

        const auto& resolvedView = framebufferManager.GetFramebufferView("ResolvedSceneColorView");
        const auto& historyView  = framebufferManager.GetFramebufferView(fmt::format("TAABufferView/{}", currentIndex));

        const auto& resolved = framebufferManager.GetFramebuffer(resolvedView.framebuffer);
        const auto& history  = framebufferManager.GetFramebuffer(historyView.framebuffer);

        Vk::BarrierWriter barrierWriter = {};

        barrierWriter
        .WriteImageBarrier(
            resolved.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstAccessMask  = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = resolved.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = resolved.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            history.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstAccessMask  = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = history.image.mipLevels,
                .baseArrayLayer = static_cast<u32>(currentIndex),
                .layerCount     = 1
            }
        )
        .Execute(cmdBuffer);

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

        vkCmdBeginRendering(cmdBuffer.handle, &renderInfo);

        pipeline.Bind(cmdBuffer);

        const VkViewport viewport =
        {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<f32>(resolved.image.width),
            .height   = static_cast<f32>(resolved.image.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vkCmdSetViewportWithCount(cmdBuffer.handle, 1, &viewport);

        const VkRect2D scissor =
        {
            .offset = {0, 0},
            .extent = {resolved.image.width, resolved.image.height}
        };

        vkCmdSetScissorWithCount(cmdBuffer.handle, 1, &scissor);

        const auto constants = TAA::Constants
        {
            .PointSamplerIndex  = textureManager.GetSampler(samplers.pointSamplerID).descriptorID,
            .LinearSamplerIndex = textureManager.GetSampler(samplers.linearSamplerID).descriptorID,
            .CurrentColorIndex  = framebufferManager.GetFramebufferView("SceneColorView").sampledImageID,
            .HistoryBufferIndex = framebufferManager.GetFramebufferView(fmt::format("TAABufferView/{}", previousIndex)).sampledImageID,
            .VelocityIndex      = framebufferManager.GetFramebufferView("GMotionVectorsView").sampledImageID,
            .SceneDepthIndex    = framebufferManager.GetFramebufferView("SceneDepthView").sampledImageID
        };

        pipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            constants
        );

        pipeline.BindDescriptors(cmdBuffer, megaSet);

        vkCmdDraw
        (
            cmdBuffer.handle,
            3,
            1,
            0,
            0
        );

        vkCmdEndRendering(cmdBuffer.handle);

        barrierWriter
        .WriteImageBarrier(
            resolved.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask  = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = resolved.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = resolved.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            history.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask  = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = history.image.mipLevels,
                .baseArrayLayer = static_cast<u32>(currentIndex),
                .layerCount     = 1
            }
        )
        .Execute(cmdBuffer);

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::ResetHistory()
    {
        m_hasToResetHistory = true;
    }
}
