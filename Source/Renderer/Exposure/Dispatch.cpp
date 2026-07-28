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

#include "Dispatch.h"

#include "Exposure/Combine.h"
#include "Exposure/Histogram.h"
#include "Exposure/Average.h"
#include "Vulkan/Constants.h"
#include "Vulkan/DebugUtils.h"
#include "Externals/ImGui.h"

namespace Renderer::Exposure
{
    Dispatch::Dispatch
    (
        const Vk::FormatHelper& formatHelper,
        const Vk::MegaSet& megaSet,
        Vk::PipelineManager& pipelineManager,
        Vk::FramebufferManager& framebufferManager
    )
    {
        constexpr std::array DYNAMIC_STATES = {VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT, VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT};

        const std::array colorFormats = {formatHelper.colorAttachmentFormatHDR};

        pipelineManager.AddPipeline(Vk::PipelineID::AutoExposureHistogram, Vk::PipelineConfig{}
            .SetPipelineType(VK_PIPELINE_BIND_POINT_COMPUTE)
            .AttachShader("Exposure/Histogram.comp", VK_SHADER_STAGE_COMPUTE_BIT)
            .AddPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Histogram::Constants))
            .AddDescriptorLayout(megaSet.layout)
        );

        pipelineManager.AddPipeline(Vk::PipelineID::AutoExposureAverage, Vk::PipelineConfig{}
            .SetPipelineType(VK_PIPELINE_BIND_POINT_COMPUTE)
            .AttachShader("Exposure/Average.comp", VK_SHADER_STAGE_COMPUTE_BIT)
            .AddPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Average::Constants))
            .AddDescriptorLayout(megaSet.layout)
        );

        pipelineManager.AddPipeline(Vk::PipelineID::AutoExposureCombine, Vk::PipelineConfig{}
           .SetPipelineType(VK_PIPELINE_BIND_POINT_GRAPHICS)
           .SetRenderingInfo(0, colorFormats, VK_FORMAT_UNDEFINED)
           .AttachShader("Misc/Triangle.vert",    VK_SHADER_STAGE_VERTEX_BIT)
           .AttachShader("Exposure/Combine.frag", VK_SHADER_STAGE_FRAGMENT_BIT)
           .SetDynamicStates(DYNAMIC_STATES)
           .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
           .SetRasterizerState(VK_FALSE, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE, VK_POLYGON_MODE_FILL)
           .AddDefaultBlendAttachment()
           .AddPushConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Combine::Constants))
           .AddDescriptorLayout(megaSet.layout)
       );

        framebufferManager.AddFramebuffer
        (
            "Exposure/Value",
            VK_FORMAT_R32_SFLOAT,
            VK_IMAGE_VIEW_TYPE_2D,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            Vk::FramebufferSize{
                .width       = 1,
                .height      = 1,
                .mipLevels   = 1,
                .arrayLayers = 1
            },
            Vk::FramebufferInitialState{
                .stageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .accessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .layout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        );

        framebufferManager.AddFramebuffer
        (
            "ExposedSceneColor",
            Vk::FramebufferCustomFormat::ColorHDR,
            VK_IMAGE_VIEW_TYPE_2D,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
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

        framebufferManager.AddFramebufferView
        (
            "Exposure/Value",
            "Exposure/ValueView",
            VK_IMAGE_VIEW_TYPE_2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        framebufferManager.AddFramebufferView
        (
            "ExposedSceneColor",
            "ExposedSceneColorView",
            VK_IMAGE_VIEW_TYPE_2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );
    }

    void Dispatch::Execute
    (
        usize FIF,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Vk::TextureManager& textureManager,
        const Buffers::ExposureBuffers& exposureBuffer,
        const Objects::Samplers& samplers,
        const Util::FrameCounter& frameCounter
    )
    {
        Vk::BeginLabel(cmdBuffer, "Auto-Exposure", glm::vec4(0.5098f, 0.6843f, 0.7549f, 1.0f));

        Histogram
        (
            cmdBuffer,
            pipelineManager,
            framebufferManager,
            megaSet,
            textureManager,
            exposureBuffer,
            samplers
        );

        Average
        (
            FIF,
            cmdBuffer,
            pipelineManager,
            framebufferManager,
            megaSet,
            exposureBuffer,
            frameCounter
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

    void Dispatch::ResetLuminance()
    {
        m_hasLuminanceBeenReset = false;
    }

    void Dispatch::Histogram
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Vk::TextureManager& textureManager,
        const Buffers::ExposureBuffers& exposureBuffer,
        const Objects::Samplers& samplers
    )
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Effects"))
            {
                if (ImGui::CollapsingHeader("Exposure"))
                {
                    ImGui::DragFloat("Adaptation Speed", &m_adaptationSpeed, 0.25f, 0.0f, 0.0f, "%.3f");
                    ImGui::DragFloat("Exposure Bias",    &m_exposureBias,    0.01f, 0.0f, 0.0f, "%.3f");

                    if (ImGui::Button("Reset Luminance"))
                    {
                        m_hasLuminanceBeenReset = false;
                    }
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        Vk::BeginLabel(cmdBuffer, "Histogram", glm::vec4(0.7098f, 0.4843f, 0.549f, 1.0f));

        const auto& histogramPipeline = pipelineManager.GetPipeline(Vk::PipelineID::AutoExposureHistogram);

        const auto& sceneColor = framebufferManager.GetFramebuffer("FinalSceneColor");

        exposureBuffer.histogramBuffer.Barrier
        (
            cmdBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .dstAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = exposureBuffer.histogramBuffer.size
            }
        );

        vkCmdFillBuffer
        (
            cmdBuffer.handle,
            exposureBuffer.histogramBuffer.handle,
            0,
            exposureBuffer.histogramBuffer.size,
            0
        );

        exposureBuffer.histogramBuffer.Barrier
        (
            cmdBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = exposureBuffer.histogramBuffer.size
            }
        );

        histogramPipeline.Bind(cmdBuffer);

        const auto constants = Histogram::Constants
        {
            .Histogram         = exposureBuffer.histogramBuffer.deviceAddress,
            .PointSamplerIndex = textureManager.GetSampler(samplers.pointSamplerID).descriptorID,
            .HDRColorIndex     = framebufferManager.GetFramebufferView("FinalSceneColorView").sampledImageID
        };

        histogramPipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_COMPUTE_BIT,
            constants
        );

        histogramPipeline.BindDescriptors(cmdBuffer, megaSet);

        vkCmdDispatch
        (
            cmdBuffer.handle,
            (sceneColor.image.width  + 16 - 1) / 16,
            (sceneColor.image.height + 16 - 1) / 16,
            1
        );

        exposureBuffer.histogramBuffer.Barrier
        (
            cmdBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = exposureBuffer.histogramBuffer.size
            }
        );

        Vk::EndLabel(cmdBuffer);
    }

    void Dispatch::Average
    (
        usize FIF,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Buffers::ExposureBuffers& exposureBuffer,
        const Util::FrameCounter& frameCounter
    )
    {
        Vk::BeginLabel(cmdBuffer, "Average", glm::vec4(0.3098f, 0.4843f, 0.549f, 1.0f));

        const auto& averagePipeline = pipelineManager.GetPipeline(Vk::PipelineID::AutoExposureAverage);

        const auto& exposureValueView = framebufferManager.GetFramebufferView("Exposure/ValueView");

        const auto& sceneColor    = framebufferManager.GetFramebuffer("FinalSceneColor");
        const auto& exposureValue = framebufferManager.GetFramebuffer(exposureValueView.framebuffer);

        if (!m_hasLuminanceBeenReset)
        {
            Vk::BarrierWriter barrierWriter = {};

            barrierWriter
            .WriteBufferBarrier(
                exposureBuffer.luminanceBuffer,
                Vk::BufferBarrier{
                    .srcStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                    .dstStageMask   = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    .dstAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .offset         = 0,
                    .size           = exposureBuffer.luminanceBuffer.size
                }
            )
            .WriteImageBarrier(
                exposureValue.image,
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
                    .levelCount     = exposureValue.image.mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount     = exposureValue.image.arrayLayers
                }
            )
            .Execute(cmdBuffer);

            constexpr f32 MIDDLE_GRAY = 0.18f;

            vkCmdFillBuffer
            (
                cmdBuffer.handle,
                exposureBuffer.luminanceBuffer.handle,
                0,
                exposureBuffer.luminanceBuffer.size,
                std::bit_cast<u32>(MIDDLE_GRAY)
            );

            constexpr VkClearColorValue RESET = {.float32 = {0.0f, 0.0f, 0.0f, 0.0f}};

            const VkImageSubresourceRange subresourceRange =
            {
                .aspectMask     = exposureValue.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = exposureValue.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = exposureValue.image.arrayLayers
            };

            vkCmdClearColorImage
            (
                cmdBuffer.handle,
                exposureValue.image.handle,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                &RESET,
                1,
                &subresourceRange
            );

            barrierWriter
            .WriteBufferBarrier(
                exposureBuffer.luminanceBuffer,
                Vk::BufferBarrier{
                    .srcStageMask   = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                    .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                    .offset         = 0,
                    .size           = exposureBuffer.luminanceBuffer.size
                }
            )
            .WriteImageBarrier(
                exposureValue.image,
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
                    .levelCount     = exposureValue.image.mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount     = exposureValue.image.arrayLayers
                }
            )
            .Execute(cmdBuffer);

            m_hasLuminanceBeenReset = true;
        }

        Vk::BarrierWriter barrierWriter = {};

        barrierWriter
        .WriteBufferBarrier(
            exposureBuffer.luminanceBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = exposureBuffer.luminanceBuffer.size
            }
        )
        .WriteImageBarrier(
            exposureValue.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_GENERAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = exposureValue.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = exposureValue.image.arrayLayers
            }
        )
        .Execute(cmdBuffer);

        averagePipeline.Bind(cmdBuffer);

        const auto constants = Average::Constants
        {
            .Histogram          = exposureBuffer.histogramBuffer.deviceAddress,
            .Luminance          = exposureBuffer.luminanceBuffer.deviceAddress,
            .ExposureImageIndex = exposureValueView.storageImageID,
            .PixelCount         = sceneColor.image.width * sceneColor.image.height,
            .TimeCoefficient    = 1.0f - std::exp(-frameCounter.frameDelta * m_adaptationSpeed),
            .ExposureBias       = m_exposureBias,
            .CurrentFrame       = static_cast<u32>(FIF),
            .PreviousFrame      = static_cast<u32>((FIF + Vk::FRAMES_IN_FLIGHT - 1) % Vk::FRAMES_IN_FLIGHT)
        };

        averagePipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_COMPUTE_BIT,
            constants
        );

        averagePipeline.BindDescriptors(cmdBuffer, megaSet);

        vkCmdDispatch
        (
            cmdBuffer.handle,
            Exposure::HISTOGRAM_SIZE,
            1,
            1
        );

        barrierWriter
        .WriteBufferBarrier(
            exposureBuffer.luminanceBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = exposureBuffer.luminanceBuffer.size
            }
        )
        .WriteImageBarrier(
            exposureValue.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_GENERAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = exposureValue.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = exposureValue.image.arrayLayers
            }
        )
        .Execute(cmdBuffer);

        Vk::EndLabel(cmdBuffer);
    }

    void Dispatch::Combine
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Vk::TextureManager& textureManager,
        const Objects::Samplers& samplers
    )
    {
        Vk::BeginLabel(cmdBuffer, "Combine", {0.8736f, 0.4598f, 0.7548f, 1.0f});

        const auto& pipeline = pipelineManager.GetPipeline(Vk::PipelineID::AutoExposureCombine);

        const auto& exposedSceneColorView = framebufferManager.GetFramebufferView("ExposedSceneColorView");
        const auto& exposedSceneColor     = framebufferManager.GetFramebuffer(exposedSceneColorView.framebuffer);

        exposedSceneColor.image.Barrier
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
                .levelCount     = exposedSceneColor.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = exposedSceneColor.image.arrayLayers
            }
        );

        const VkRenderingAttachmentInfo colorAttachmentInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = exposedSceneColorView.view.handle,
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
                .offset = {.x     = 0,                             .y      = 0                             },
                .extent = {.width = exposedSceneColor.image.width, .height = exposedSceneColor.image.height}
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
            .width    = static_cast<f32>(exposedSceneColor.image.width),
            .height   = static_cast<f32>(exposedSceneColor.image.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vkCmdSetViewportWithCount(cmdBuffer.handle, 1, &viewport);

        const VkRect2D scissor =
        {
            .offset = {.x     = 0,                             .y      = 0                             },
            .extent = {.width = exposedSceneColor.image.width, .height = exposedSceneColor.image.height}
        };

        vkCmdSetScissorWithCount(cmdBuffer.handle, 1, &scissor);

        const auto constants = Combine::Constants
        {
            .PointSamplerIndex = textureManager.GetSampler(samplers.pointSamplerID).descriptorID,
            .SceneColorIndex   = framebufferManager.GetFramebufferView("ResolvedSceneColorView").sampledImageID,
            .ExposureIndex     = framebufferManager.GetFramebufferView("Exposure/ValueView").sampledImageID
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

        exposedSceneColor.image.Barrier
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
                .levelCount     = exposedSceneColor.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = exposedSceneColor.image.arrayLayers
            }
        );

        Vk::EndLabel(cmdBuffer);
    }
}
