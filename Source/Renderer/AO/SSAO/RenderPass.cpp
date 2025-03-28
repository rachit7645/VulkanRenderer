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

#include "Vulkan/DebugUtils.h"
#include "Util/Log.h"
#include "Util/Random.h"
#include "Util/SIMD.h"
#include "Renderer/RenderConstants.h"

namespace Renderer::AO::SSAO
{
    RenderPass::RenderPass
    (
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        Vk::FramebufferManager& framebufferManager,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager
    )
        : occlusionPipeline(context, formatHelper, megaSet, textureManager),
          blurHorizontalPipeline(context, formatHelper, megaSet, textureManager),
          blurVerticalPipeline(context, formatHelper, megaSet, textureManager),
          sampleBuffer(context)
    {
        for (usize i = 0; i < cmdBuffers.size(); ++i)
        {
            cmdBuffers[i] = Vk::CommandBuffer
            (
                context.device,
                context.commandPool,
                VK_COMMAND_BUFFER_LEVEL_PRIMARY
            );

            Vk::SetDebugName(context.device, cmdBuffers[i].handle, fmt::format("SSAOPass/FIF{}", i));
        }

        framebufferManager.AddFramebuffer
        (
            "Occlusion",
            Vk::FramebufferType::ColorR,
            Vk::ImageType::Single2D,
            false,
            [] (const VkExtent2D& extent) -> Vk::FramebufferSize
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
            "OcclusionBlurHorizontal",
            Vk::FramebufferType::ColorR,
            Vk::ImageType::Single2D,
            false,
            [] (const VkExtent2D& extent) -> Vk::FramebufferSize
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
            "OcclusionBlurVertical",
            Vk::FramebufferType::ColorR,
            Vk::ImageType::Single2D,
            false,
            [] (const VkExtent2D& extent) -> Vk::FramebufferSize
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

        framebufferManager.AddFramebufferView
        (
            "Occlusion",
            "OcclusionView",
            Vk::ImageType::Single2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        framebufferManager.AddFramebufferView
        (
            "OcclusionBlurHorizontal",
            "OcclusionBlurHorizontalView",
            Vk::ImageType::Single2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        framebufferManager.AddFramebufferView
        (
            "OcclusionBlurVertical",
            "OcclusionBlurVerticalView",
            Vk::ImageType::Single2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        constexpr u32 NOISE_COUNT = 16;

        std::array<glm::vec2, NOISE_COUNT> ssaoNoise = {};

        for (usize i = 0; i < ssaoNoise.size(); ++i)
        {
            ssaoNoise[i] =
            {
                Util::TrueRandRange(0.0f, 1.0f) * 2.0f - 1.0f,
                Util::TrueRandRange(0.0f, 1.0f) * 2.0f - 1.0f
            };
        }

        std::array<f16, 2 * NOISE_COUNT> ssaoNoiseHalf = {};
        Util::ConvertF32ToF16Range(&ssaoNoise[0][0], ssaoNoiseHalf.data(), 0, ssaoNoiseHalf.size());

        noiseTexture = textureManager.AddTexture
        (
            megaSet,
            context.device,
            context.allocator,
            "SSAONoise",
            std::span(reinterpret_cast<u8*>(ssaoNoiseHalf.data()), ssaoNoiseHalf.size() * sizeof(u16)),
            {std::sqrt(NOISE_COUNT), std::sqrt(NOISE_COUNT)},
            formatHelper.rgFloatFormat
        );

        Logger::Info("{}\n", "Created SSAO pass!");
    }

    void RenderPass::Render
    (
        usize FIF,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Buffers::SceneBuffer& sceneBuffer
    )
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Occlusion"))
            {
                ImGui::DragFloat("Radius", &m_radius, 0.005f,  0.0f, 1.0f, "%.3f");
                ImGui::DragFloat("Bias",   &m_bias,   0.0005f, 0.0f, 1.0f, "%.4f");
                ImGui::DragFloat("Power",  &m_power,  0.05f,   0.0f, 0.0f, "%.3f");

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        const auto& currentCmdBuffer = cmdBuffers[FIF];

        currentCmdBuffer.Reset(0);
        currentCmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        Vk::BeginLabel(currentCmdBuffer, fmt::format("SSAOPass/FIF{}", FIF), glm::vec4(0.9098f, 0.2843f, 0.7529f, 1.0f));

        RenderOcclusion
        (
            FIF,
            currentCmdBuffer,
            framebufferManager,
            megaSet,
            sceneBuffer
        );

        RenderBlurHorizontal
        (
            currentCmdBuffer,
            framebufferManager,
            megaSet
        );

        RenderBlurVertical
        (
            currentCmdBuffer,
            framebufferManager,
            megaSet
        );

        Vk::EndLabel(currentCmdBuffer);

        currentCmdBuffer.EndRecording();
    }

    void RenderPass::RenderOcclusion
    (
        usize FIF,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Buffers::SceneBuffer& sceneBuffer
    )
    {
        Vk::BeginLabel(cmdBuffer, "Occlusion", glm::vec4(0.3098f, 0.7843f, 0.7529f, 1.0f));

        const auto& colorAttachmentView = framebufferManager.GetFramebufferView("OcclusionView");
        const auto& colorAttachment     = framebufferManager.GetFramebuffer(colorAttachmentView.framebuffer);

        colorAttachment.image.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            {
                .aspectMask     = colorAttachment.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = colorAttachment.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = colorAttachment.image.arrayLayers
            }
        );

        const VkRenderingAttachmentInfo colorAttachmentInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = colorAttachmentView.view.handle,
            .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode        = VK_RESOLVE_MODE_NONE,
            .resolveImageView   = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp             = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue         = {}
        };

        const VkRenderingInfo renderInfo =
        {
            .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext                = nullptr,
            .flags                = 0,
            .renderArea           = {
                .offset = {0, 0},
                .extent = {colorAttachment.image.width, colorAttachment.image.height}
            },
            .layerCount           = 1,
            .viewMask             = 0,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &colorAttachmentInfo,
            .pDepthAttachment     = nullptr,
            .pStencilAttachment   = nullptr
        };

        vkCmdBeginRendering(cmdBuffer.handle, &renderInfo);

        occlusionPipeline.Bind(cmdBuffer);

        const VkViewport viewport =
        {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<f32>(colorAttachment.image.width),
            .height   = static_cast<f32>(colorAttachment.image.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vkCmdSetViewportWithCount(cmdBuffer.handle, 1, &viewport);

        const VkRect2D scissor =
        {
            .offset = {0, 0},
            .extent = {colorAttachment.image.width, colorAttachment.image.height}
        };

        vkCmdSetScissorWithCount(cmdBuffer.handle, 1, &scissor);

        occlusionPipeline.pushConstant =
        {
            .scene               = sceneBuffer.buffers[FIF].deviceAddress,
            .samples             = sampleBuffer.buffer.deviceAddress,
            .gBufferSamplerIndex = occlusionPipeline.gBufferSamplerIndex,
            .noiseSamplerIndex   = occlusionPipeline.noiseSamplerIndex,
            .gNormalIndex        = framebufferManager.GetFramebufferView("GNormal_Rgh_Mtl_View").sampledImageIndex,
            .sceneDepthIndex     = framebufferManager.GetFramebufferView("SceneDepthView").sampledImageIndex,
            .noiseIndex          = noiseTexture,
            .radius              = m_radius,
            .bias                = m_bias,
            .power               = m_power
        };

        occlusionPipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(Occlusion::PushConstant),
            &occlusionPipeline.pushConstant
        );

        const std::array descriptorSets = {megaSet.descriptorSet};
        occlusionPipeline.BindDescriptors(cmdBuffer, 0, descriptorSets);

        vkCmdDraw
        (
            cmdBuffer.handle,
            3,
            1,
            0,
            0
        );

        vkCmdEndRendering(cmdBuffer.handle);

        colorAttachment.image.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            {
                .aspectMask     = colorAttachment.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = colorAttachment.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = colorAttachment.image.arrayLayers
            }
        );

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::RenderBlurHorizontal
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet
    )
    {
        Vk::BeginLabel(cmdBuffer, "Blur/Horizontal", glm::vec4(0.7098f, 0.3823f, 0.2129f, 1.0f));

        const auto& colorAttachmentView = framebufferManager.GetFramebufferView("OcclusionBlurHorizontalView");
        const auto& colorAttachment     = framebufferManager.GetFramebuffer(colorAttachmentView.framebuffer);

        colorAttachment.image.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            {
                .aspectMask     = colorAttachment.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = colorAttachment.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = colorAttachment.image.arrayLayers
            }
        );

        const VkRenderingAttachmentInfo colorAttachmentInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = colorAttachmentView.view.handle,
            .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode        = VK_RESOLVE_MODE_NONE,
            .resolveImageView   = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp             = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue         = {}
        };

        const VkRenderingInfo renderInfo =
        {
            .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext                = nullptr,
            .flags                = 0,
            .renderArea           = {
                .offset = {0, 0},
                .extent = {colorAttachment.image.width, colorAttachment.image.height}
            },
            .layerCount           = 1,
            .viewMask             = 0,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &colorAttachmentInfo,
            .pDepthAttachment     = nullptr,
            .pStencilAttachment   = nullptr
        };

        vkCmdBeginRendering(cmdBuffer.handle, &renderInfo);

        blurHorizontalPipeline.Bind(cmdBuffer);

        const VkViewport viewport =
        {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<f32>(colorAttachment.image.width),
            .height   = static_cast<f32>(colorAttachment.image.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vkCmdSetViewportWithCount(cmdBuffer.handle, 1, &viewport);

        const VkRect2D scissor =
        {
            .offset = {0, 0},
            .extent = {colorAttachment.image.width, colorAttachment.image.height}
        };

        vkCmdSetScissorWithCount(cmdBuffer.handle, 1, &scissor);

        blurHorizontalPipeline.pushConstant =
        {
            .samplerIndex = blurHorizontalPipeline.samplerIndex,
            .imageIndex   = framebufferManager.GetFramebufferView("OcclusionView").sampledImageIndex
        };

        blurHorizontalPipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(Blur::PushConstant),
            &blurHorizontalPipeline.pushConstant
        );

        const std::array descriptorSets = {megaSet.descriptorSet};
        blurHorizontalPipeline.BindDescriptors(cmdBuffer, 0, descriptorSets);

        vkCmdDraw
        (
            cmdBuffer.handle,
            3,
            1,
            0,
            0
        );

        vkCmdEndRendering(cmdBuffer.handle);

        colorAttachment.image.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            {
                .aspectMask     = colorAttachment.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = colorAttachment.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = colorAttachment.image.arrayLayers
            }
        );

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::RenderBlurVertical
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet
    )
    {
        Vk::BeginLabel(cmdBuffer, "Blur/Vertical", glm::vec4(0.7098f, 0.6823f, 0.1129f, 1.0f));

        const auto& colorAttachmentView = framebufferManager.GetFramebufferView("OcclusionBlurVerticalView");
        const auto& colorAttachment     = framebufferManager.GetFramebuffer(colorAttachmentView.framebuffer);

        colorAttachment.image.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            {
                .aspectMask     = colorAttachment.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = colorAttachment.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = colorAttachment.image.arrayLayers
            }
        );

        const VkRenderingAttachmentInfo colorAttachmentInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = colorAttachmentView.view.handle,
            .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode        = VK_RESOLVE_MODE_NONE,
            .resolveImageView   = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp             = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue         = {}
        };

        const VkRenderingInfo renderInfo =
        {
            .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext                = nullptr,
            .flags                = 0,
            .renderArea           = {
                .offset = {0, 0},
                .extent = {colorAttachment.image.width, colorAttachment.image.height}
            },
            .layerCount           = 1,
            .viewMask             = 0,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &colorAttachmentInfo,
            .pDepthAttachment     = nullptr,
            .pStencilAttachment   = nullptr
        };

        vkCmdBeginRendering(cmdBuffer.handle, &renderInfo);

        blurVerticalPipeline.Bind(cmdBuffer);

        const VkViewport viewport =
        {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<f32>(colorAttachment.image.width),
            .height   = static_cast<f32>(colorAttachment.image.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vkCmdSetViewportWithCount(cmdBuffer.handle, 1, &viewport);

        const VkRect2D scissor =
        {
            .offset = {0, 0},
            .extent = {colorAttachment.image.width, colorAttachment.image.height}
        };

        vkCmdSetScissorWithCount(cmdBuffer.handle, 1, &scissor);

        blurVerticalPipeline.pushConstant =
        {
            .samplerIndex = blurVerticalPipeline.samplerIndex,
            .imageIndex   = framebufferManager.GetFramebufferView("OcclusionBlurHorizontalView").sampledImageIndex
        };

        blurVerticalPipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(Blur::PushConstant),
            &blurVerticalPipeline.pushConstant
        );

        const std::array descriptorSets = {megaSet.descriptorSet};
        blurVerticalPipeline.BindDescriptors(cmdBuffer, 0, descriptorSets);

        vkCmdDraw
        (
            cmdBuffer.handle,
            3,
            1,
            0,
            0
        );

        vkCmdEndRendering(cmdBuffer.handle);

        colorAttachment.image.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            {
                .aspectMask     = colorAttachment.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = colorAttachment.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = colorAttachment.image.arrayLayers
            }
        );

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::Destroy(VkDevice device, VmaAllocator allocator, VkCommandPool cmdPool)
    {
        Logger::Debug("{}\n", "Destroying SSAO pass!");

        sampleBuffer.Destroy(allocator);

        Vk::CommandBuffer::Free(device, cmdPool, cmdBuffers);

        occlusionPipeline.Destroy(device);
        blurHorizontalPipeline.Destroy(device);
        blurVerticalPipeline.Destroy(device);
    }
}
