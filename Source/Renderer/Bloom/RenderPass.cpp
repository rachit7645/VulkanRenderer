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

#include "Renderer/RenderConstants.h"
#include "Vulkan/DebugUtils.h"
#include "Util/Log.h"

namespace Renderer::Bloom
{
    RenderPass::RenderPass
    (
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        Vk::FramebufferManager& framebufferManager,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager
    )
        : downsamplePipeline(context, formatHelper, megaSet, textureManager),
          upsamplePipeline(context, formatHelper, megaSet, textureManager)
    {
        framebufferManager.AddFramebuffer
        (
            "Bloom",
            Vk::FramebufferType::ColorHDR,
            Vk::FramebufferImageType::Array2D,
            Vk::FramebufferUsage::Attachment | Vk::FramebufferUsage::Sampled,
            [device = context.device, &framebufferManager, &megaSet] (const VkExtent2D& extent, Util::DeletionQueue& deletionQueue) -> Vk::FramebufferSize
            {
                framebufferManager.DeleteFramebufferViews
                (
                    "Bloom",
                    device,
                    megaSet,
                    deletionQueue
                );

                const auto size = Vk::FramebufferSize
                {
                    .width       = extent.width,
                    .height      = extent.height,
                    .mipLevels   = static_cast<u32>(std::floor(std::log2(std::max(extent.width, extent.height)))) + 1,
                    .arrayLayers = 1
                };

                for (u32 mipLevel = 0; mipLevel < size.mipLevels; ++mipLevel)
                {
                    framebufferManager.AddFramebufferView
                    (
                        "Bloom",
                        fmt::format("BloomView/{}", mipLevel),
                        Vk::FramebufferImageType::Single2D,
                        {
                            .baseMipLevel   = mipLevel,
                            .levelCount     = 1,
                            .baseArrayLayer = 0,
                            .layerCount     = 1
                        }
                    );
                }

                return size;
            },
            {
                .dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        );

        Logger::Info("{}\n", "Created bloom pass!");
    }

    void RenderPass::Render
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet
    )
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Bloom"))
            {
                ImGui::DragFloat("Filter Radius", &m_filterRadius, 0.0005f, 0.0f, 0.1f, "%.4f");

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        Vk::BeginLabel(cmdBuffer, "BloomPass", {0.6796f, 0.4538f, 0.1518f, 1.0f});

        RenderDownSamples
        (
            cmdBuffer,
            framebufferManager,
            megaSet
        );

        RenderUpSamples
        (
            cmdBuffer,
            framebufferManager,
            megaSet
        );

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::RenderDownSamples
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet
    )
    {
        Vk::BeginLabel(cmdBuffer, "DownSamplePass", {0.7796f, 0.3588f, 0.5518f, 1.0f});

        const auto& bloomBuffer = framebufferManager.GetFramebuffer("Bloom");

        auto srcView = framebufferManager.GetFramebufferView("ResolvedSceneColorView");
        auto dstView = framebufferManager.GetFramebufferView("BloomView/0");

        for (u32 mip = 0; mip < bloomBuffer.image.mipLevels; ++mip)
        {
            Vk::BeginLabel(cmdBuffer, fmt::format("Mip #{}", mip), {0.5882f, 0.9294f, 0.2117f, 1.0f});

            bloomBuffer.image.Barrier
            (
                cmdBuffer,
                Vk::ImageBarrier{
                    .srcStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                    .dstStageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .dstAccessMask  = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .newLayout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .baseMipLevel   = mip,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = bloomBuffer.image.arrayLayers
                }
            );

            const auto mipWidth  = std::max(static_cast<u32>(bloomBuffer.image.width  * std::pow(0.5f, mip)), 1u);
            const auto mipHeight = std::max(static_cast<u32>(bloomBuffer.image.height * std::pow(0.5f, mip)), 1u);

            const VkRenderingAttachmentInfo colorAttachmentInfo =
            {
                .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext              = nullptr,
                .imageView          = dstView.view.handle,
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
                    .extent = {mipWidth, mipHeight}
                },
                .layerCount           = 1,
                .viewMask             = 0,
                .colorAttachmentCount = 1,
                .pColorAttachments    = &colorAttachmentInfo,
                .pDepthAttachment     = nullptr,
                .pStencilAttachment   = nullptr
            };

            vkCmdBeginRendering(cmdBuffer.handle, &renderInfo);

            downsamplePipeline.Bind(cmdBuffer);

            const VkViewport viewport =
            {
                .x        = 0.0f,
                .y        = 0.0f,
                .width    = static_cast<f32>(mipWidth),
                .height   = static_cast<f32>(mipHeight),
                .minDepth = 0.0f,
                .maxDepth = 1.0f
            };

            vkCmdSetViewportWithCount(cmdBuffer.handle, 1, &viewport);

            const VkRect2D scissor =
            {
                .offset = {0, 0},
                .extent = {mipWidth, mipHeight}
            };

            vkCmdSetScissorWithCount(cmdBuffer.handle, 1, &scissor);

            downsamplePipeline.pushConstant =
            {
                .samplerIndex  = downsamplePipeline.samplerIndex,
                .imageIndex    = srcView.sampledImageIndex,
                .isFirstSample = (mip == 0) ? 1u : 0u
            };

            downsamplePipeline.PushConstants
            (
               cmdBuffer,
               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
               0, sizeof(DownSample::PushConstant),
               reinterpret_cast<void*>(&downsamplePipeline.pushConstant)
            );

            std::array descriptorSets = {megaSet.descriptorSet};
            downsamplePipeline.BindDescriptors(cmdBuffer, 0, descriptorSets);

            vkCmdDraw
            (
                cmdBuffer.handle,
                3,
                1,
                0,
                0
            );

            vkCmdEndRendering(cmdBuffer.handle);

            bloomBuffer.image.Barrier
            (
                cmdBuffer,
                Vk::ImageBarrier{
                    .srcStageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .srcAccessMask  = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                    .oldLayout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .baseMipLevel   = mip,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = bloomBuffer.image.arrayLayers
                }
            );

            if (u32 nextMip = (mip + 1); nextMip < bloomBuffer.image.mipLevels)
            {
                srcView = dstView;
                dstView = framebufferManager.GetFramebufferView(fmt::format("BloomView/{}", nextMip));
            }

            Vk::EndLabel(cmdBuffer);
        }

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::RenderUpSamples
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet
    )
    {
        Vk::BeginLabel(cmdBuffer, "UpSamplePass", {0.8736f, 0.2598f, 0.7548f, 1.0f});

        const auto& bloomBuffer = framebufferManager.GetFramebuffer("Bloom");

        for (u32 mip = bloomBuffer.image.mipLevels - 1; mip > 0; --mip)
        {
            Vk::BeginLabel(cmdBuffer, fmt::format("Mip #{}", mip), {0.5882f, 0.9294f, 0.2117f, 1.0f});

            bloomBuffer.image.Barrier
            (
                cmdBuffer,
                Vk::ImageBarrier{
                    .srcStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                    .dstStageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .dstAccessMask  = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .newLayout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .baseMipLevel   = mip - 1,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = bloomBuffer.image.arrayLayers
                }
            );

            auto srcView = framebufferManager.GetFramebufferView(fmt::format("BloomView/{}", mip));
            auto dstView = framebufferManager.GetFramebufferView(fmt::format("BloomView/{}", mip - 1));

            const auto mipWidth  = std::max(static_cast<u32>(bloomBuffer.image.width  * std::pow(0.5f, mip - 1)), 1u);
            const auto mipHeight = std::max(static_cast<u32>(bloomBuffer.image.height * std::pow(0.5f, mip - 1)), 1u);

            const VkRenderingAttachmentInfo colorAttachmentInfo =
            {
                .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext              = nullptr,
                .imageView          = dstView.view.handle,
                .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .resolveMode        = VK_RESOLVE_MODE_NONE,
                .resolveImageView   = VK_NULL_HANDLE,
                .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .loadOp             = VK_ATTACHMENT_LOAD_OP_LOAD,
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
                    .extent = {mipWidth, mipHeight}
                },
                .layerCount           = 1,
                .viewMask             = 0,
                .colorAttachmentCount = 1,
                .pColorAttachments    = &colorAttachmentInfo,
                .pDepthAttachment     = nullptr,
                .pStencilAttachment   = nullptr
            };

            vkCmdBeginRendering(cmdBuffer.handle, &renderInfo);

            upsamplePipeline.Bind(cmdBuffer);

            const VkViewport viewport =
            {
                .x        = 0.0f,
                .y        = 0.0f,
                .width    = static_cast<f32>(mipWidth),
                .height   = static_cast<f32>(mipHeight),
                .minDepth = 0.0f,
                .maxDepth = 1.0f
            };

            vkCmdSetViewportWithCount(cmdBuffer.handle, 1, &viewport);

            const VkRect2D scissor =
            {
                .offset = {0, 0},
                .extent = {mipWidth, mipHeight}
            };

            vkCmdSetScissorWithCount(cmdBuffer.handle, 1, &scissor);

            upsamplePipeline.pushConstant =
            {
                .samplerIndex  = upsamplePipeline.samplerIndex,
                .imageIndex    = srcView.sampledImageIndex,
                .filterRadius  = m_filterRadius
            };

            upsamplePipeline.PushConstants
            (
               cmdBuffer,
               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
               0, sizeof(DownSample::PushConstant),
               reinterpret_cast<void*>(&upsamplePipeline.pushConstant)
            );

            std::array descriptorSets = {megaSet.descriptorSet};
            upsamplePipeline.BindDescriptors(cmdBuffer, 0, descriptorSets);

            vkCmdDraw
            (
                cmdBuffer.handle,
                3,
                1,
                0,
                0
            );

            vkCmdEndRendering(cmdBuffer.handle);

            bloomBuffer.image.Barrier
            (
                cmdBuffer,
                Vk::ImageBarrier{
                    .srcStageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                    .srcAccessMask  = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                    .oldLayout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .baseMipLevel   = mip - 1,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = bloomBuffer.image.arrayLayers
                }
            );

            Vk::EndLabel(cmdBuffer);
        }

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::Destroy(VkDevice device)
    {
        Logger::Debug("{}\n", "Destroying bloom pass!");

        downsamplePipeline.Destroy(device);
        upsamplePipeline.Destroy(device);
    }
}
