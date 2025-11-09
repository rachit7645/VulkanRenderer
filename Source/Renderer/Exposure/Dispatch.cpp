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

#include "Dispatch.h"

#include "Exposure/Histogram.h"
#include "Exposure/Average.h"
#include "GPU/Constants.h"
#include "Vulkan/DebugUtils.h"

namespace Renderer::Exposure
{
    Dispatch::Dispatch(const Vk::MegaSet& megaSet, Vk::PipelineManager& pipelineManager)
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
    }

    void Dispatch::Execute
    (
        usize FIF,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Vk::TextureManager& textureManager,
        const Buffers::ExposureBuffer& exposureBuffer,
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
            exposureBuffer,
            frameCounter
        );

        Vk::EndLabel(cmdBuffer);
    }

    void Dispatch::Histogram
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Vk::TextureManager& textureManager,
        const Buffers::ExposureBuffer& exposureBuffer,
        const Objects::GlobalSamplers& samplers
    )
    {
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
        const Buffers::ExposureBuffer& exposureBuffer,
        const Util::FrameCounter& frameCounter
    )
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Exposure"))
            {
                ImGui::DragFloat("Adaptation Speed", &m_adaptationSpeed, 0.25f, 0.0f, 0.0f, "%.3f");

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        Vk::BeginLabel(cmdBuffer, "Average", glm::vec4(0.3098f, 0.4843f, 0.549f, 1.0f));

        const auto& averagePipeline = pipelineManager.GetPipeline("Exposure/Average");

        const auto& sceneColor = framebufferManager.GetFramebuffer("FinalSceneColor");

        if (!m_hasLuminanceBeenReset)
        {
            exposureBuffer.luminanceBuffer.Barrier
            (
                cmdBuffer,
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
            );

            vkCmdFillBuffer
            (
                cmdBuffer.handle,
                exposureBuffer.luminanceBuffer.handle,
                0,
                exposureBuffer.luminanceBuffer.size,
                0
            );

            exposureBuffer.luminanceBuffer.Barrier
            (
                cmdBuffer,
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
            );

            m_hasLuminanceBeenReset = true;
        }

        exposureBuffer.luminanceBuffer.Barrier
        (
            cmdBuffer,
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
        );

        averagePipeline.Bind(cmdBuffer);

        const auto constants = Average::Constants
        {
            .Histogram       = exposureBuffer.histogramBuffer.deviceAddress,
            .Luminance       = exposureBuffer.luminanceBuffer.deviceAddress,
            .PixelCount      = sceneColor.image.width * sceneColor.image.height,
            .TimeCoefficient = 1.0f - std::exp(-frameCounter.frameDelta * m_adaptationSpeed),
            .CurrentFrame    = static_cast<u32>(FIF),
            .PreviousFrame   = static_cast<u32>((FIF + Vk::FRAMES_IN_FLIGHT - 1) % Vk::FRAMES_IN_FLIGHT)
        };

        averagePipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_COMPUTE_BIT,
            constants
        );

        vkCmdDispatch
        (
            cmdBuffer.handle,
            Exposure::HISTOGRAM_SIZE,
            1,
            1
        );

        exposureBuffer.luminanceBuffer.Barrier
        (
            cmdBuffer,
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
        );

        Vk::EndLabel(cmdBuffer);
    }
}
