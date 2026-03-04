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

#include "Exposure/Histogram.h"
#include "Exposure/Average.h"
#include "GPU/Constants.h"
#include "Vulkan/DebugUtils.h"
#include "Externals/ImGui.h"

namespace Renderer::Exposure
{
    Dispatch::Dispatch
    (
        const Vk::MegaSet& megaSet,
        Vk::PipelineManager& pipelineManager,
        Vk::FramebufferManager& framebufferManager
    )
    {
        pipelineManager.AddPipeline("Exposure/Histogram", Vk::PipelineConfig{}
            .SetPipelineType(VK_PIPELINE_BIND_POINT_COMPUTE)
            .AttachShader("Exposure/Histogram.comp", VK_SHADER_STAGE_COMPUTE_BIT)
            .AddPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Histogram::Constants))
            .AddDescriptorLayout(megaSet.descriptorLayout)
        );

        pipelineManager.AddPipeline("Exposure/Average", Vk::PipelineConfig{}
            .SetPipelineType(VK_PIPELINE_BIND_POINT_COMPUTE)
            .AttachShader("Exposure/Average.comp", VK_SHADER_STAGE_COMPUTE_BIT)
            .AddPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Average::Constants))
            .AddDescriptorLayout(megaSet.descriptorLayout)
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
        const Objects::GlobalSamplers& samplers,
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
        const Objects::GlobalSamplers& samplers
    )
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Exposure"))
            {
                ImGui::DragFloat("Adaptation Speed", &m_adaptationSpeed, 0.25f, 0.0f, 0.0f, "%.3f");
                ImGui::DragFloat("Exposure Bias",    &m_exposureBias,    0.01f, 0.0f, 0.0f, "%.3f");

                if (ImGui::Button("Reset Luminance"))
                {
                    m_hasLuminanceBeenReset = false;
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        Vk::BeginLabel(cmdBuffer, "Histogram", glm::vec4(0.7098f, 0.4843f, 0.549f, 1.0f));

        const auto& histogramPipeline = pipelineManager.GetPipeline("Exposure/Histogram");

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

        const auto& averagePipeline = pipelineManager.GetPipeline("Exposure/Average");

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

            vkCmdFillBuffer
            (
                cmdBuffer.handle,
                exposureBuffer.luminanceBuffer.handle,
                0,
                exposureBuffer.luminanceBuffer.size,
                0
            );

            constexpr VkClearColorValue RESET = {.float32 = {1.0f, 0.0f, 0.0f, 0.0f}};

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
}
