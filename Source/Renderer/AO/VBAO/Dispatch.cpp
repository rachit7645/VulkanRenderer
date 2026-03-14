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

#include "AO/VBAO/DepthPreFilter.h"
#include "AO/VBAO/SpacialDenoise.h"
#include "AO/VBAO/VBAO.h"
#include "Util/Log.h"
#include "Vulkan/DebugUtils.h"
#include "Externals/ImGui.h"

namespace Renderer::AO::VBAO
{
    Dispatch::Dispatch
    (
        const Vk::MegaSet& megaSet,
        Vk::PipelineManager& pipelineManager,
        Vk::FramebufferManager& framebufferManager
    )
    {
        pipelineManager.AddPipeline("VBAO/DepthPreFilter", Vk::PipelineConfig{}
            .SetPipelineType(VK_PIPELINE_BIND_POINT_COMPUTE)
            .AttachShader("AO/VBAO/DepthPreFilter.comp", VK_SHADER_STAGE_COMPUTE_BIT)
            .AddPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(DepthPreFilter::Constants))
            .AddDescriptorLayout(megaSet.descriptorLayout)
        );

        pipelineManager.AddPipeline("VBAO/Occlusion", Vk::PipelineConfig{}
            .SetPipelineType(VK_PIPELINE_BIND_POINT_COMPUTE)
            .AttachShader("AO/VBAO/VBAO.comp", VK_SHADER_STAGE_COMPUTE_BIT)
            .AddPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Occlusion::Constants))
            .AddDescriptorLayout(megaSet.descriptorLayout)
        );

        pipelineManager.AddPipeline("VBAO/Denoise", Vk::PipelineConfig{}
            .SetPipelineType(VK_PIPELINE_BIND_POINT_COMPUTE)
            .AttachShader("AO/VBAO/SpacialDenoise.comp", VK_SHADER_STAGE_COMPUTE_BIT)
            .AddPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Denoise::Constants))
            .AddDescriptorLayout(megaSet.descriptorLayout)
        );

        framebufferManager.AddFramebuffer
        (
            "VBAO/DepthMipChain",
            VK_FORMAT_R32_SFLOAT,
            VK_IMAGE_VIEW_TYPE_2D,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            [] (const VkExtent2D& renderExtent, ENGINE_UNUSED const VkExtent2D& displayExtent) -> Vk::FramebufferSize
            {
                return
                {
                    .width       = renderExtent.width,
                    .height      = renderExtent.height,
                    .mipLevels   = Occlusion::VBAO_DEPTH_MIP_LEVELS,
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
            "VBAO/DepthMipChain",
            "VBAO/DepthMipChainView",
            VK_IMAGE_VIEW_TYPE_2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = Occlusion::VBAO_DEPTH_MIP_LEVELS,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        for (u32 i = 0; i < Occlusion::VBAO_DEPTH_MIP_LEVELS; ++i)
        {
            framebufferManager.AddFramebufferView
            (
                "VBAO/DepthMipChain",
                fmt::format("VBAO/DepthMipChainView/Mip{}", i),
                VK_IMAGE_VIEW_TYPE_2D,
                Vk::FramebufferViewSize{
                    .baseMipLevel   = i,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1
                }
            );
        }

        framebufferManager.AddFramebuffer
        (
            "VBAO/DepthDifferences",
            VK_FORMAT_R32_UINT,
            VK_IMAGE_VIEW_TYPE_2D,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            [] (const VkExtent2D& renderExtent, ENGINE_UNUSED const VkExtent2D& displayExtent) -> Vk::FramebufferSize
            {
                return
                {
                    .width       = renderExtent.width,
                    .height      = renderExtent.height,
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
            "VBAO/DepthDifferences",
            "VBAO/DepthDifferencesView",
            VK_IMAGE_VIEW_TYPE_2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        framebufferManager.AddFramebuffer
        (
            "VBAO/NoisyAO",
            VK_FORMAT_R16_UNORM,
            VK_IMAGE_VIEW_TYPE_2D,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            [] (const VkExtent2D& renderExtent, ENGINE_UNUSED const VkExtent2D& displayExtent) -> Vk::FramebufferSize
            {
                return
                {
                    .width       = renderExtent.width,
                    .height      = renderExtent.height,
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
            "VBAO/NoisyAO",
            "VBAO/NoisyAOView",
            VK_IMAGE_VIEW_TYPE_2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        framebufferManager.AddFramebuffer
        (
            "VBAO/Occlusion",
            VK_FORMAT_R16_UNORM,
            VK_IMAGE_VIEW_TYPE_2D,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            [] (const VkExtent2D& renderExtent, ENGINE_UNUSED const VkExtent2D& displayExtent) -> Vk::FramebufferSize
            {
                return
                {
                    .width       = renderExtent.width,
                    .height      = renderExtent.height,
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
            "VBAO/Occlusion",
            "VBAO/OcclusionView",
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
        usize frameIndex,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Vk::TextureManager& textureManager,
        const Buffers::SceneBuffer& sceneBuffer,
        const Objects::GlobalSamplers& samplers,
        const std::string_view sceneDepthID,
        const std::string_view gNormalID
    )
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Effects"))
            {
                if (ImGui::CollapsingHeader("VBAO"))
                {
                    ImGui::DragFloat("Power",     &m_finalValuePower, 0.05f,  0.0f, 0.0f, "%.4f");
                    ImGui::DragFloat("Thickness", &m_thickness,       0.005f, 0.0f, 1.0f, "%.4f");
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        Vk::BeginLabel(cmdBuffer, "VBAO", glm::vec4(0.9098f, 0.2843f, 0.7529f, 1.0f));

        PreFilterDepth
        (
            cmdBuffer,
            pipelineManager,
            framebufferManager,
            megaSet,
            textureManager,
            samplers,
            sceneDepthID
        );

        Occlusion
        (
            FIF,
            frameIndex,
            cmdBuffer,
            pipelineManager,
            framebufferManager,
            megaSet,
            textureManager,
            sceneBuffer,
            samplers,
            gNormalID
        );

        Denoise
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

    void Dispatch::PreFilterDepth
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Vk::TextureManager& textureManager,
        const Objects::GlobalSamplers& samplers,
        const std::string_view sceneDepthID
    )
    {
        Vk::BeginLabel(cmdBuffer, "DepthPreFilter", glm::vec4(0.6098f, 0.2143f, 0.4529f, 1.0f));

        const auto& depthPreFilterPipeline = pipelineManager.GetPipeline("VBAO/DepthPreFilter");

        const auto& depthMipChain = framebufferManager.GetFramebuffer("VBAO/DepthMipChain");

        depthMipChain.image.Barrier
        (
            cmdBuffer,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_GENERAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = depthMipChain.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = depthMipChain.image.arrayLayers
            }
        );

        depthPreFilterPipeline.Bind(cmdBuffer);

        const auto constants = DepthPreFilter::Constants
        {
            .PointSamplerIndex = textureManager.GetSampler(samplers.pointSamplerID).descriptorID,
            .SceneDepthIndex   = framebufferManager.GetFramebufferView(sceneDepthID).sampledImageID,
            .OutDepthMip0Index = framebufferManager.GetFramebufferView("VBAO/DepthMipChainView/Mip0").storageImageID,
            .OutDepthMip1Index = framebufferManager.GetFramebufferView("VBAO/DepthMipChainView/Mip1").storageImageID,
            .OutDepthMip2Index = framebufferManager.GetFramebufferView("VBAO/DepthMipChainView/Mip2").storageImageID,
            .OutDepthMip3Index = framebufferManager.GetFramebufferView("VBAO/DepthMipChainView/Mip3").storageImageID,
            .OutDepthMip4Index = framebufferManager.GetFramebufferView("VBAO/DepthMipChainView/Mip4").storageImageID,
        };

        depthPreFilterPipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_COMPUTE_BIT,
            constants
        );

        depthPreFilterPipeline.BindDescriptors(cmdBuffer, megaSet);

        vkCmdDispatch
        (
            cmdBuffer.handle,
            (depthMipChain.image.width  + 16 - 1) / 16,
            (depthMipChain.image.height + 16 - 1) / 16,
            1
        );

        depthMipChain.image.Barrier
        (
            cmdBuffer,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_GENERAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = depthMipChain.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = depthMipChain.image.arrayLayers
            }
        );

        Vk::EndLabel(cmdBuffer);
    }

    void Dispatch::Occlusion
    (
        usize FIF,
        usize frameIndex,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Vk::TextureManager& textureManager,
        const Buffers::SceneBuffer& sceneBuffer,
        const Objects::GlobalSamplers& samplers,
        const std::string_view gNormalID
    )
    {
        Vk::BeginLabel(cmdBuffer, "Occlusion", glm::vec4(0.6098f, 0.7143f, 0.4529f, 1.0f));

        const auto& occlusionPipeline = pipelineManager.GetPipeline("VBAO/Occlusion");

        const auto& noisyAO          = framebufferManager.GetFramebuffer("VBAO/NoisyAO");
        const auto& depthDifferences = framebufferManager.GetFramebuffer("VBAO/DepthDifferences");

        Vk::BarrierWriter barrierWriter = {};

        barrierWriter
        .WriteImageBarrier(
            noisyAO.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_GENERAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = noisyAO.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        )
        .WriteImageBarrier(
            depthDifferences.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_GENERAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = depthDifferences.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = depthDifferences.image.arrayLayers
            }
        )
        .Execute(cmdBuffer);

        occlusionPipeline.Bind(cmdBuffer);

        const auto constants = Occlusion::Constants
        {
            .Scene                    = sceneBuffer.buffers[FIF].deviceAddress,
            .PointSamplerIndex        = textureManager.GetSampler(samplers.pointSamplerID).descriptorID,
            .LinearSamplerIndex       = textureManager.GetSampler(samplers.linearSamplerID).descriptorID,
            .HilbertLUTIndex          = textureManager.GetTexture(hilbertLUT).descriptorID,
            .GNormalIndex             = framebufferManager.GetFramebufferView(gNormalID).sampledImageID,
            .PreFilterDepthIndex      = framebufferManager.GetFramebufferView("VBAO/DepthMipChainView").sampledImageID,
            .OutDepthDifferencesIndex = framebufferManager.GetFramebufferView("VBAO/DepthDifferencesView").storageImageID,
            .OutNoisyAOIndex          = framebufferManager.GetFramebufferView("VBAO/NoisyAOView").storageImageID,
            .TemporalIndex            = static_cast<u32>(frameIndex % BASE_JITTER_PHASE_COUNT),
            .Thickness                = m_thickness
        };

        occlusionPipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_COMPUTE_BIT,
            constants
        );

        occlusionPipeline.BindDescriptors(cmdBuffer, megaSet);

        vkCmdDispatch
        (
            cmdBuffer.handle,
            (noisyAO.image.width  + 8 - 1) / 8,
            (noisyAO.image.height + 8 - 1) / 8,
            1
        );

        barrierWriter
        .WriteImageBarrier(
            noisyAO.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_GENERAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = noisyAO.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        )
        .WriteImageBarrier(
            depthDifferences.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_GENERAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = depthDifferences.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = depthDifferences.image.arrayLayers
            }
        )
        .Execute(cmdBuffer);

        Vk::EndLabel(cmdBuffer);
    }

    void Dispatch::Denoise
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Vk::TextureManager& textureManager,
        const Objects::GlobalSamplers& samplers
    )
    {
        Vk::BeginLabel(cmdBuffer, "Denoise", glm::vec4(0.2098f, 0.2143f, 0.7859f, 1.0f));

        const auto& denoisePipeline = pipelineManager.GetPipeline("VBAO/Denoise");

        const auto& occlusion = framebufferManager.GetFramebuffer("VBAO/Occlusion");

        denoisePipeline.Bind(cmdBuffer);

        const auto constants = Denoise::Constants
        {
            .PointSamplerIndex     = textureManager.GetSampler(samplers.pointSamplerID).descriptorID,
            .DepthDifferencesIndex = framebufferManager.GetFramebufferView("VBAO/DepthDifferencesView").sampledImageID,
            .NoisyAOIndex          = framebufferManager.GetFramebufferView("VBAO/NoisyAOView").sampledImageID,
            .OutAOIndex            = framebufferManager.GetFramebufferView("VBAO/OcclusionView").storageImageID,
            .FinalValuePower       = m_finalValuePower
        };

        denoisePipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_COMPUTE_BIT,
            constants
        );

        denoisePipeline.BindDescriptors(cmdBuffer, megaSet);

        occlusion.image.Barrier
        (
           cmdBuffer,
           Vk::ImageBarrier{
               .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
               .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
               .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
               .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
               .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
               .newLayout      = VK_IMAGE_LAYOUT_GENERAL,
               .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
               .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
               .baseMipLevel   = 0,
               .levelCount     = occlusion.image.mipLevels,
               .baseArrayLayer = 0,
               .layerCount     = occlusion.image.arrayLayers
           }
        );

        vkCmdDispatch
        (
            cmdBuffer.handle,
            (occlusion.image.width  + 8 - 1) / 8,
            (occlusion.image.height + 8 - 1) / 8,
            1
        );

        occlusion.image.Barrier
        (
           cmdBuffer,
           Vk::ImageBarrier{
               .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
               .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
               .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
               .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
               .oldLayout      = VK_IMAGE_LAYOUT_GENERAL,
               .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
               .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
               .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
               .baseMipLevel   = 0,
               .levelCount     = occlusion.image.mipLevels,
               .baseArrayLayer = 0,
               .layerCount     = occlusion.image.arrayLayers
           }
        );

        Vk::EndLabel(cmdBuffer);
    }
}
