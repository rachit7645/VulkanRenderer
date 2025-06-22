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

#include "Util/Log.h"
#include "Vulkan/DebugUtils.h"
#include "AO/VBGTAO/DepthPreFilter.h"
#include "AO/VBGTAO/SpacialDenoise.h"
#include "AO/VBGTAO/VBGTAO.h"

namespace Renderer::AO::VBGTAO
{
    Dispatch::Dispatch
    (
        const Vk::Context& context,
        const Vk::MegaSet& megaSet,
        Vk::FramebufferManager& framebufferManager
    )
        : m_depthPreFilterPipeline(context, megaSet),
          m_occlusionPipeline(context, megaSet),
          m_denoisePipeline(context, megaSet)
    {
        framebufferManager.AddFramebuffer
        (
            "VBGTAO/DepthMipChain",
            Vk::FramebufferType::ColorR_SFloat32,
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferUsage::Sampled | Vk::FramebufferUsage::Storage,
            [] (const VkExtent2D& extent) -> Vk::FramebufferSize
            {
                return
                {
                    .width       = extent.width,
                    .height      = extent.height,
                    .mipLevels   = Occlusion::GTAO_DEPTH_MIP_LEVELS,
                    .arrayLayers = 1
                };
            },
            {
                .dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        );

        framebufferManager.AddFramebufferView
        (
            "VBGTAO/DepthMipChain",
            "VBGTAO/DepthMipChainView",
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = Occlusion::GTAO_DEPTH_MIP_LEVELS,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        for (u32 i = 0; i < Occlusion::GTAO_DEPTH_MIP_LEVELS; ++i)
        {
            framebufferManager.AddFramebufferView
            (
                "VBGTAO/DepthMipChain",
                fmt::format("VBGTAO/DepthMipChainView/Mip{}", i),
                Vk::FramebufferImageType::Single2D,
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
            "VBGTAO/DepthDifferences",
            Vk::FramebufferType::ColorR_Uint32,
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferUsage::Sampled | Vk::FramebufferUsage::Storage,
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
            {
                .dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        );

        framebufferManager.AddFramebufferView
        (
            "VBGTAO/DepthDifferences",
            "VBGTAO/DepthDifferencesView",
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        framebufferManager.AddFramebuffer
        (
            "VBGTAO/NoisyAO",
            Vk::FramebufferType::ColorR_Unorm16,
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferUsage::Sampled | Vk::FramebufferUsage::Storage,
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
            {
                .dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        );

        framebufferManager.AddFramebufferView
        (
            "VBGTAO/NoisyAO",
            "VBGTAO/NoisyAOView",
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        framebufferManager.AddFramebuffer
        (
            "VBGTAO/Occlusion",
            Vk::FramebufferType::ColorR_Unorm16,
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferUsage::Sampled | Vk::FramebufferUsage::Storage,
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
            {
                .dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        );

        framebufferManager.AddFramebufferView
        (
            "VBGTAO/Occlusion",
            "VBGTAO/OcclusionView",
            Vk::FramebufferImageType::Single2D,
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
            if (ImGui::BeginMenu("VBGTAO"))
            {
                ImGui::DragFloat("Power",     &m_finalValuePower, 0.05f,  0.0f, 0.0f, "%.4f");
                ImGui::DragFloat("Thickness", &m_thickness,       0.005f, 0.0f, 1.0f, "%.4f");

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        Vk::BeginLabel(cmdBuffer, "VBGTAO", glm::vec4(0.9098f, 0.2843f, 0.7529f, 1.0f));

        PreFilterDepth
        (
            cmdBuffer,
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
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Vk::TextureManager& textureManager,
        const Objects::GlobalSamplers& samplers,
        const std::string_view sceneDepthID
    )
    {
        Vk::BeginLabel(cmdBuffer, "DepthPreFilter", glm::vec4(0.6098f, 0.2143f, 0.4529f, 1.0f));

        const auto& depthMipChain = framebufferManager.GetFramebuffer("VBGTAO/DepthMipChain");

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

        m_depthPreFilterPipeline.Bind(cmdBuffer);

        const auto constants = DepthPreFilter::Constants
        {
            .PointSamplerIndex = textureManager.GetSampler(samplers.pointSamplerID).descriptorID,
            .SceneDepthIndex   = framebufferManager.GetFramebufferView(sceneDepthID).sampledImageID,
            .OutDepthMip0Index = framebufferManager.GetFramebufferView("VBGTAO/DepthMipChainView/Mip0").storageImageID,
            .OutDepthMip1Index = framebufferManager.GetFramebufferView("VBGTAO/DepthMipChainView/Mip1").storageImageID,
            .OutDepthMip2Index = framebufferManager.GetFramebufferView("VBGTAO/DepthMipChainView/Mip2").storageImageID,
            .OutDepthMip3Index = framebufferManager.GetFramebufferView("VBGTAO/DepthMipChainView/Mip3").storageImageID,
            .OutDepthMip4Index = framebufferManager.GetFramebufferView("VBGTAO/DepthMipChainView/Mip4").storageImageID,
        };

        m_depthPreFilterPipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_COMPUTE_BIT,
            constants
        );

        const std::array descriptorSets = {megaSet.descriptorSet};
        m_depthPreFilterPipeline.BindDescriptors(cmdBuffer, 0, descriptorSets);

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
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Vk::TextureManager& textureManager,
        const Buffers::SceneBuffer& sceneBuffer,
        const Objects::GlobalSamplers& samplers,
        const std::string_view gNormalID
    )
    {
        Vk::BeginLabel(cmdBuffer, "Occlusion", glm::vec4(0.6098f, 0.7143f, 0.4529f, 1.0f));

        const auto& noisyAO          = framebufferManager.GetFramebuffer("VBGTAO/NoisyAO");
        const auto& depthDifferences = framebufferManager.GetFramebuffer("VBGTAO/DepthDifferences");

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

        m_occlusionPipeline.Bind(cmdBuffer);

        const auto constants = Occlusion::Constants
        {
            .Scene                    = sceneBuffer.buffers[FIF].deviceAddress,
            .PointSamplerIndex        = textureManager.GetSampler(samplers.pointSamplerID).descriptorID,
            .LinearSamplerIndex       = textureManager.GetSampler(samplers.linearSamplerID).descriptorID,
            .HilbertLUTIndex          = textureManager.GetTexture(hilbertLUT).descriptorID,
            .GNormalIndex             = framebufferManager.GetFramebufferView(gNormalID).sampledImageID,
            .PreFilterDepthIndex      = framebufferManager.GetFramebufferView("VBGTAO/DepthMipChainView").sampledImageID,
            .OutDepthDifferencesIndex = framebufferManager.GetFramebufferView("VBGTAO/DepthDifferencesView").storageImageID,
            .OutNoisyAOIndex          = framebufferManager.GetFramebufferView("VBGTAO/NoisyAOView").storageImageID,
            .TemporalIndex            = static_cast<u32>(frameIndex % JITTER_SAMPLE_COUNT),
            .Thickness                = m_thickness
        };

        m_occlusionPipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_COMPUTE_BIT,
            constants
        );

        const std::array descriptorSets = {megaSet.descriptorSet};
        m_occlusionPipeline.BindDescriptors(cmdBuffer, 0, descriptorSets);

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
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Vk::TextureManager& textureManager,
        const Objects::GlobalSamplers& samplers
    )
    {
        Vk::BeginLabel(cmdBuffer, "Denoise", glm::vec4(0.2098f, 0.2143f, 0.7859f, 1.0f));

        const auto& occlusion = framebufferManager.GetFramebuffer("VBGTAO/Occlusion");

        m_denoisePipeline.Bind(cmdBuffer);

        const auto constants = Denoise::Constants
        {
            .PointSamplerIndex     = textureManager.GetSampler(samplers.pointSamplerID).descriptorID,
            .DepthDifferencesIndex = framebufferManager.GetFramebufferView("VBGTAO/DepthDifferencesView").sampledImageID,
            .NoisyAOIndex          = framebufferManager.GetFramebufferView("VBGTAO/NoisyAOView").sampledImageID,
            .OutAOIndex            = framebufferManager.GetFramebufferView("VBGTAO/OcclusionView").storageImageID,
            .FinalValuePower       = m_finalValuePower
        };

        m_denoisePipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_COMPUTE_BIT,
            constants
        );

        const std::array descriptorSets = {megaSet.descriptorSet};
        m_denoisePipeline.BindDescriptors(cmdBuffer, 0, descriptorSets);

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

    void Dispatch::Destroy(VkDevice device)
    {
        m_depthPreFilterPipeline.Destroy(device);
        m_occlusionPipeline.Destroy(device);
        m_denoisePipeline.Destroy(device);
    }
}
