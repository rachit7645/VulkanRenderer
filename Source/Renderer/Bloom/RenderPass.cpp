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

#include "Bloom/DownSample.h"
#include "Bloom/UpSample.h"
#include "Bloom/Combine.h"
#include "Renderer/RenderConstants.h"
#include "Vulkan/DebugUtils.h"
#include "Util/Log.h"

namespace Renderer::Bloom
{
    RenderPass::RenderPass
    (
        VkDevice device,
        const Vk::FormatHelper& formatHelper,
        Vk::MegaSet& megaSet,
        Vk::PipelineManager& pipelineManager,
        Vk::FramebufferManager& framebufferManager
    )
    {
        constexpr std::array DYNAMIC_STATES = {VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT, VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT};

        const std::array colorFormats = {formatHelper.colorAttachmentFormatHDR};

        pipelineManager.AddPipeline("Bloom/DownSample/FirstSample", Vk::PipelineConfig{}
            .SetPipelineType(VK_PIPELINE_BIND_POINT_GRAPHICS)
            .SetRenderingInfo(0, colorFormats, VK_FORMAT_UNDEFINED)
            .AttachShader("Misc/Triangle.vert",                VK_SHADER_STAGE_VERTEX_BIT)
            .AttachShader("Bloom/DownSample/FirstSample.frag", VK_SHADER_STAGE_FRAGMENT_BIT)
            .SetDynamicStates(DYNAMIC_STATES)
            .SetIAState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetRasterizerState(VK_FALSE, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_FILL)
            .AddDefaultBlendAttachment()
            .AddPushConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DownSample::Constants))
            .AddDescriptorLayout(megaSet.descriptorLayout)
        );

        pipelineManager.AddPipeline("Bloom/DownSample/Regular", Vk::PipelineConfig{}
            .SetPipelineType(VK_PIPELINE_BIND_POINT_GRAPHICS)
            .SetRenderingInfo(0, colorFormats, VK_FORMAT_UNDEFINED)
            .AttachShader("Misc/Triangle.vert",             VK_SHADER_STAGE_VERTEX_BIT)
            .AttachShader("Bloom/DownSample/Regular.frag", VK_SHADER_STAGE_FRAGMENT_BIT)
            .SetDynamicStates(DYNAMIC_STATES)
            .SetIAState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetRasterizerState(VK_FALSE, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_FILL)
            .AddDefaultBlendAttachment()
            .AddPushConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DownSample::Constants))
            .AddDescriptorLayout(megaSet.descriptorLayout)
        );

        pipelineManager.AddPipeline("Bloom/UpSample", Vk::PipelineConfig{}
            .SetPipelineType(VK_PIPELINE_BIND_POINT_GRAPHICS)
            .SetRenderingInfo(0, colorFormats, VK_FORMAT_UNDEFINED)
            .AttachShader("Misc/Triangle.vert",   VK_SHADER_STAGE_VERTEX_BIT)
            .AttachShader("Bloom/UpSample.frag", VK_SHADER_STAGE_FRAGMENT_BIT)
            .SetDynamicStates(DYNAMIC_STATES)
            .SetIAState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetRasterizerState(VK_FALSE, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_FILL)
            .AddBlendAttachment(
                VK_TRUE,
                VK_BLEND_FACTOR_ONE,
                VK_BLEND_FACTOR_ONE,
                VK_BLEND_OP_ADD,
                VK_BLEND_FACTOR_ONE,
                VK_BLEND_FACTOR_ONE,
                VK_BLEND_OP_ADD,
                VK_COLOR_COMPONENT_R_BIT |
                VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT |
                VK_COLOR_COMPONENT_A_BIT
            )
            .AddPushConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(UpSample::Constants))
            .AddDescriptorLayout(megaSet.descriptorLayout)
        );

        pipelineManager.AddPipeline("Bloom/Combine", Vk::PipelineConfig{}
            .SetPipelineType(VK_PIPELINE_BIND_POINT_GRAPHICS)
            .SetRenderingInfo(0, colorFormats, VK_FORMAT_UNDEFINED)
            .AttachShader("Misc/Triangle.vert",  VK_SHADER_STAGE_VERTEX_BIT)
            .AttachShader("Bloom/Combine.frag", VK_SHADER_STAGE_FRAGMENT_BIT)
            .SetDynamicStates(DYNAMIC_STATES)
            .SetIAState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetRasterizerState(VK_FALSE, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_FILL)
            .AddDefaultBlendAttachment()
            .AddPushConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Combine::Constants))
            .AddDescriptorLayout(megaSet.descriptorLayout)
        );

        framebufferManager.AddFramebuffer
        (
            "Bloom",
            Vk::FramebufferType::ColorHDR,
            Vk::FramebufferImageType::Array2D,
            Vk::FramebufferUsage::Attachment | Vk::FramebufferUsage::Sampled,
            [device, &framebufferManager, &megaSet] (const VkExtent2D& extent, Util::DeletionQueue& deletionQueue) -> Vk::FramebufferSize
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
            Vk::FramebufferInitialState{
                .dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        );

        framebufferManager.AddFramebuffer
        (
            "FinalSceneColor",
            Vk::FramebufferType::ColorHDR,
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferUsage::Attachment | Vk::FramebufferUsage::Sampled,
            [] (const VkExtent2D& extent) -> Vk::FramebufferSize
            {
                return
                {
                    .width       = extent.width,
                    .height      = extent.height,
                    .mipLevels   = 1,
                    .arrayLayers = 1
                };
            },
            Vk::FramebufferInitialState{
                .dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        );

        framebufferManager.AddFramebufferView
        (
            "FinalSceneColor",
            "FinalSceneColorView",
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );
    }

    void RenderPass::Render
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Vk::TextureManager& textureManager,
        const Objects::GlobalSamplers& samplers
    )
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Bloom"))
            {
                ImGui::DragFloat("Filter Radius", &m_filterRadius,  0.0005f,  0.0f, 0.1f, "%.4f");
                ImGui::DragFloat("Strength",      &m_bloomStrength, 0.00125f, 0.0f, 1.0f, "%.4f");

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        Vk::BeginLabel(cmdBuffer, "Bloom", {0.6796f, 0.4538f, 0.1518f, 1.0f});

        DownSample
        (
            cmdBuffer,
            pipelineManager,
            framebufferManager,
            megaSet,
            textureManager,
            samplers
        );

        UpSample
        (
            cmdBuffer,
            pipelineManager,
            framebufferManager,
            megaSet,
            textureManager,
            samplers
        );

        Combine
        (
            cmdBuffer,
            pipelineManager,
            framebufferManager,
            megaSet,
            textureManager,
            samplers
        );

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::DownSample
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Vk::TextureManager& textureManager,
        const Objects::GlobalSamplers& samplers
    )
    {
        Vk::BeginLabel(cmdBuffer, "DownSample", {0.7796f, 0.3588f, 0.5518f, 1.0f});

        const auto& downSampleFirstSamplePipeline = pipelineManager.GetPipeline("Bloom/DownSample/FirstSample");
        const auto& downSampleRegularPipeline     = pipelineManager.GetPipeline("Bloom/DownSample/Regular");

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
                    .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
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

            const auto constants = DownSample::Constants
            {
                .SamplerIndex  = textureManager.GetSampler(samplers.linearSamplerID).descriptorID,
                .ImageIndex    = srcView.sampledImageID
            };

            if (mip == 0)
            {
                downSampleFirstSamplePipeline.Bind(cmdBuffer);

                downSampleFirstSamplePipeline.PushConstants
                (
                   cmdBuffer,
                   VK_SHADER_STAGE_FRAGMENT_BIT,
                   constants
                );

                downSampleFirstSamplePipeline.BindDescriptors(cmdBuffer, megaSet);
            }
            else
            {
                if (mip == 1)
                {
                    downSampleRegularPipeline.Bind(cmdBuffer);
                    downSampleRegularPipeline.BindDescriptors(cmdBuffer, megaSet);
                }

                downSampleRegularPipeline.PushConstants
                (
                   cmdBuffer,
                   VK_SHADER_STAGE_FRAGMENT_BIT,
                   constants
                );
            }

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
                    .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
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

    void RenderPass::UpSample
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Vk::TextureManager& textureManager,
        const Objects::GlobalSamplers& samplers
    )
    {
        Vk::BeginLabel(cmdBuffer, "UpSample", {0.8736f, 0.2598f, 0.7548f, 1.0f});

        const auto& upsamplePipeline = pipelineManager.GetPipeline("Bloom/UpSample");

        const auto& bloomBuffer = framebufferManager.GetFramebuffer("Bloom");

        upsamplePipeline.Bind(cmdBuffer);
        upsamplePipeline.BindDescriptors(cmdBuffer, megaSet);

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
                    .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
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

            const auto constants = UpSample::Constants
            {
                .SamplerIndex  = textureManager.GetSampler(samplers.linearSamplerID).descriptorID,
                .ImageIndex    = srcView.sampledImageID,
                .FilterRadius  = m_filterRadius
            };

            upsamplePipeline.PushConstants
            (
               cmdBuffer,
               VK_SHADER_STAGE_FRAGMENT_BIT,
               constants
            );

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
                    .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
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

    void RenderPass::Combine
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Vk::TextureManager& textureManager,
        const Objects::GlobalSamplers& samplers
    )
    {
        Vk::BeginLabel(cmdBuffer, "Combine", {0.8736f, 0.4598f, 0.7548f, 1.0f});

        const auto& combinePipeline = pipelineManager.GetPipeline("Bloom/Combine");

        const auto& finalSceneColorView = framebufferManager.GetFramebufferView("FinalSceneColorView");
        const auto& finalSceneColor     = framebufferManager.GetFramebuffer(finalSceneColorView.framebuffer);

        finalSceneColor.image.Barrier
        (
            cmdBuffer,
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
                .levelCount     = finalSceneColor.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = finalSceneColor.image.arrayLayers
            }
        );

        const VkRenderingAttachmentInfo colorAttachmentInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = finalSceneColorView.view.handle,
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
                .extent = {finalSceneColor.image.width, finalSceneColor.image.height}
            },
            .layerCount           = 1,
            .viewMask             = 0,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &colorAttachmentInfo,
            .pDepthAttachment     = nullptr,
            .pStencilAttachment   = nullptr
        };

        vkCmdBeginRendering(cmdBuffer.handle, &renderInfo);

        combinePipeline.Bind(cmdBuffer);

        const VkViewport viewport =
        {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<f32>(finalSceneColor.image.width),
            .height   = static_cast<f32>(finalSceneColor.image.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vkCmdSetViewportWithCount(cmdBuffer.handle, 1, &viewport);

        const VkRect2D scissor =
        {
            .offset = {0, 0},
            .extent = {finalSceneColor.image.width, finalSceneColor.image.height}
        };

        vkCmdSetScissorWithCount(cmdBuffer.handle, 1, &scissor);

        const auto constants = Combine::Constants
        {
            .PointSamplerIndex = textureManager.GetSampler(samplers.pointSamplerID).descriptorID,
            .SceneColorIndex   = framebufferManager.GetFramebufferView("ResolvedSceneColorView").sampledImageID,
            .BloomIndex        = framebufferManager.GetFramebufferView("BloomView/0").sampledImageID,
            .BloomStrength     = m_bloomStrength
        };

        combinePipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            constants
        );

        combinePipeline.BindDescriptors(cmdBuffer, megaSet);

        vkCmdDraw
        (
            cmdBuffer.handle,
            3,
            1,
            0,
            0
        );

        vkCmdEndRendering(cmdBuffer.handle);

        finalSceneColor.image.Barrier
        (
            cmdBuffer,
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
                .levelCount     = finalSceneColor.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = finalSceneColor.image.arrayLayers
            }
        );

        Vk::EndLabel(cmdBuffer);
    }
}
