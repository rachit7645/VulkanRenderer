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

#include "Renderer/Depth/RenderPass.h"
#include "Renderer/RenderConstants.h"
#include "Vulkan/DebugUtils.h"
#include "Util/Log.h"
#include "Util/Maths.h"

namespace Renderer::AO::XeGTAO
{
    constexpr u32   XE_GTAO_DEPTH_MIP_LEVELS        = 5;
    constexpr u32   XE_GTAO_HILBERT_LEVEL           = 6;                           // Must match the value in Constants.glsl
    constexpr u32   XE_GTAO_HILBERT_WIDTH           = 1u << XE_GTAO_HILBERT_LEVEL; // Must match the value in Constants.glsl
    constexpr usize XE_GTA0_DENOISE_PASS_COUNT      = 1;                           // Must be at least 1
    constexpr u32   XE_GTA0_WORKING_AO_HISTORY_SIZE = (XE_GTA0_DENOISE_PASS_COUNT == 1) ? 1 : 2;

    constexpr glm::uvec2 XE_GTAO_NUM_THREADS = {8, 8};

    RenderPass::RenderPass
    (
        const Vk::Context& context,
        Vk::FramebufferManager& framebufferManager,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager
    )
        : depthPreFilterPipeline(context, megaSet, textureManager),
          occlusionPipeline(context, megaSet, textureManager),
          denoisePipeline(context, megaSet, textureManager)
    {
        framebufferManager.AddFramebuffer
        (
            "XeGTAO/DepthMipChain",
            Vk::FramebufferType::Depth,
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferUsage::Sampled | Vk::FramebufferUsage::Storage,
            [] (const VkExtent2D& extent) -> Vk::FramebufferSize
            {
                return
                {
                    .width       = extent.width,
                    .height      = extent.height,
                    .mipLevels   = XE_GTAO_DEPTH_MIP_LEVELS,
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
            "XeGTAO/DepthMipChain",
            "XeGTAO/DepthMipChainView",
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = XE_GTAO_DEPTH_MIP_LEVELS,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        for (u32 i = 0; i < XE_GTAO_DEPTH_MIP_LEVELS; ++i)
        {
            framebufferManager.AddFramebufferView
            (
                "XeGTAO/DepthMipChain",
                fmt::format("XeGTAO/DepthMipChainView/Mip{}", i),
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
            "XeGTAO/Edges",
            Vk::FramebufferType::ColorR_Unorm8,
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
            "XeGTAO/Edges",
            "XeGTAO/EdgesView",
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
            "XeGTAO/WorkingAO",
            Vk::FramebufferType::ColorR_Unorm8,
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferUsage::Sampled | Vk::FramebufferUsage::Storage,
            [] (const VkExtent2D& extent) -> Vk::FramebufferSize
            {
                return
                {
                    .width       = extent.width,
                    .height      = extent.height,
                    .mipLevels   = 1,
                    .arrayLayers = XE_GTA0_WORKING_AO_HISTORY_SIZE
                };
            },
            {
                .dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        );

        for (u32 i = 0; i < XE_GTA0_WORKING_AO_HISTORY_SIZE; ++i)
        {
            framebufferManager.AddFramebufferView
            (
                "XeGTAO/WorkingAO",
                fmt::format("XeGTAO/WorkingAOView/{}", i),
                Vk::FramebufferImageType::Single2D,
                Vk::FramebufferViewSize{
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = i,
                    .layerCount     = 1
                }
            );
        }

        framebufferManager.AddFramebuffer
        (
            "XeGTAO/Occlusion",
            Vk::FramebufferType::ColorR_Unorm8,
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
            "XeGTAO/Occlusion",
            "XeGTAO/OcclusionView",
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        Logger::Info("{}\n", "Created XeGTAO pass!");
    }

    void RenderPass::Render
    (
        usize FIF,
        usize frameIndex,
        const Vk::CommandBuffer& cmdBuffer,
        VkDevice device,
        VmaAllocator allocator,
        const Vk::FormatHelper& formatHelper,
        const Vk::FramebufferManager& framebufferManager,
        const Buffers::SceneBuffer& sceneBuffer,
        Vk::MegaSet& megaSet,
        Models::ModelManager& modelManager,
        Util::DeletionQueue& deletionQueue
    )
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("XeGTAO"))
            {
                ImGui::DragFloat("Power", &m_finalValuePower, 0.05f, 0.0f, 0.0f, "%.4f");

                // Power must not be negative
                m_finalValuePower = std::max(m_finalValuePower, 0.0f);

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        Vk::BeginLabel(cmdBuffer, fmt::format("XeGTAOPass/FIF{}", FIF), glm::vec4(0.9098f, 0.2843f, 0.7529f, 1.0f));

        if (!hilbertLUT.has_value())
        {
            constexpr auto HILBERT_SEQUENCE = Maths::GenerateHilbertSequence<XE_GTAO_HILBERT_LEVEL>();

            hilbertLUT = modelManager.textureManager.AddTexture
            (
                device,
                allocator,
                megaSet,
                deletionQueue,
                "XeGTAO/HilbertLUT",
                formatHelper.rUint16Format,
                HILBERT_SEQUENCE.data(),
                XE_GTAO_HILBERT_WIDTH,
                XE_GTAO_HILBERT_WIDTH
            );

            modelManager.Update(cmdBuffer, device, allocator, deletionQueue);
            megaSet.Update(device);
        }

        PreFilterDepth(cmdBuffer, framebufferManager, megaSet);

        Occlusion
        (
            FIF,
            frameIndex,
            cmdBuffer,
            framebufferManager,
            megaSet,
            sceneBuffer
        );

        Denoise(cmdBuffer, framebufferManager, megaSet);

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::PreFilterDepth
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet
    )
    {
        Vk::BeginLabel(cmdBuffer, "DepthPreFilter", glm::vec4(0.6098f, 0.2143f, 0.4529f, 1.0f));

        const auto& depthMipChain = framebufferManager.GetFramebuffer("XeGTAO/DepthMipChain");

        depthMipChain.image.Barrier
        (
            cmdBuffer,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_GENERAL,
                .baseMipLevel   = 0,
                .levelCount     = depthMipChain.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = depthMipChain.image.arrayLayers
            }
        );

        depthPreFilterPipeline.Bind(cmdBuffer);

        depthPreFilterPipeline.pushConstant =
        {
            .depthSamplerIndex = depthPreFilterPipeline.samplerIndex,
            .sceneDepthIndex   = framebufferManager.GetFramebufferView("SceneDepthView").sampledImageIndex,
            .outDepthMip0Index = framebufferManager.GetFramebufferView("XeGTAO/DepthMipChainView/Mip0").storageImageIndex,
            .outDepthMip1Index = framebufferManager.GetFramebufferView("XeGTAO/DepthMipChainView/Mip1").storageImageIndex,
            .outDepthMip2Index = framebufferManager.GetFramebufferView("XeGTAO/DepthMipChainView/Mip2").storageImageIndex,
            .outDepthMip3Index = framebufferManager.GetFramebufferView("XeGTAO/DepthMipChainView/Mip3").storageImageIndex,
            .outDepthMip4Index = framebufferManager.GetFramebufferView("XeGTAO/DepthMipChainView/Mip4").storageImageIndex,
        };

        depthPreFilterPipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(DepthPreFilter::PushConstant),
            &depthPreFilterPipeline.pushConstant
        );

        const std::array descriptorSets = {megaSet.descriptorSet};
        depthPreFilterPipeline.BindDescriptors(cmdBuffer, 0, descriptorSets);

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
                .baseMipLevel   = 0,
                .levelCount     = depthMipChain.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = depthMipChain.image.arrayLayers
            }
        );

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::Occlusion
    (
        usize FIF,
        usize frameIndex,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Buffers::SceneBuffer& sceneBuffer
    )
    {
        Vk::BeginLabel(cmdBuffer, "Occlusion", glm::vec4(0.6098f, 0.7143f, 0.4529f, 1.0f));

        const auto& workingAO = framebufferManager.GetFramebuffer("XeGTAO/WorkingAO");
        const auto& outEdges  = framebufferManager.GetFramebuffer("XeGTAO/Edges");

        Vk::BarrierWriter barrierWriter = {};

        barrierWriter
        .WriteImageBarrier(
            workingAO.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_GENERAL,
                .baseMipLevel   = 0,
                .levelCount     = workingAO.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        )
        .WriteImageBarrier(
            outEdges.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_GENERAL,
                .baseMipLevel   = 0,
                .levelCount     = outEdges.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = outEdges.image.arrayLayers
            }
        )
        .Execute(cmdBuffer);

        occlusionPipeline.Bind(cmdBuffer);

        occlusionPipeline.pushConstant =
        {
            .scene               = sceneBuffer.buffers[FIF].deviceAddress,
            .samplerIndex        = occlusionPipeline.samplerIndex,
            .hilbertLUTIndex     = hilbertLUT.value(),
            .gNormalIndex        = framebufferManager.GetFramebufferView("GNormal_Rgh_Mtl_View").sampledImageIndex,
            .viewSpaceDepthIndex = framebufferManager.GetFramebufferView("XeGTAO/DepthMipChainView").sampledImageIndex,
            .outWorkingEdges     = framebufferManager.GetFramebufferView("XeGTAO/EdgesView").storageImageIndex,
            .outWorkingAOIndex   = framebufferManager.GetFramebufferView("XeGTAO/WorkingAOView/0").storageImageIndex,
            .temporalIndex       = static_cast<u32>(frameIndex % JITTER_SAMPLE_COUNT),
            .finalValuePower     = m_finalValuePower
        };

        occlusionPipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(Occlusion::PushConstant),
            &occlusionPipeline.pushConstant
        );

        const std::array descriptorSets = {megaSet.descriptorSet};
        occlusionPipeline.BindDescriptors(cmdBuffer, 0, descriptorSets);

        vkCmdDispatch
        (
            cmdBuffer.handle,
            (workingAO.image.width  + XE_GTAO_NUM_THREADS.x - 1) / XE_GTAO_NUM_THREADS.x,
            (workingAO.image.height + XE_GTAO_NUM_THREADS.y - 1) / XE_GTAO_NUM_THREADS.y,
            1
        );

        barrierWriter
        .WriteImageBarrier(
            workingAO.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_GENERAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .baseMipLevel   = 0,
                .levelCount     = workingAO.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        )
        .WriteImageBarrier(
            outEdges.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_GENERAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .baseMipLevel   = 0,
                .levelCount     = outEdges.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = outEdges.image.arrayLayers
            }
        )
        .Execute(cmdBuffer);

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::Denoise
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet
    )
    {
        Vk::BeginLabel(cmdBuffer, "Denoise", glm::vec4(0.2098f, 0.2143f, 0.7859f, 1.0f));

        const auto& workingAO = framebufferManager.GetFramebuffer("XeGTAO/WorkingAO");

        denoisePipeline.Bind(cmdBuffer);

        const std::array descriptorSets = {megaSet.descriptorSet};
        denoisePipeline.BindDescriptors(cmdBuffer, 0, descriptorSets);

        for (usize i = 1; i <= XE_GTA0_DENOISE_PASS_COUNT; ++i)
        {
            const bool finalApply = (i == XE_GTA0_DENOISE_PASS_COUNT);

            const u32 previousIndex = (i + XE_GTA0_WORKING_AO_HISTORY_SIZE - 1) % XE_GTA0_WORKING_AO_HISTORY_SIZE;
            const u32 currentIndex  = i % XE_GTA0_WORKING_AO_HISTORY_SIZE;

            const auto& previousView = framebufferManager.GetFramebufferView(fmt::format("XeGTAO/WorkingAOView/{}", previousIndex));
            const auto& currentView  = finalApply ? framebufferManager.GetFramebufferView("XeGTAO/OcclusionView") : framebufferManager.GetFramebufferView(fmt::format("XeGTAO/WorkingAOView/{}", currentIndex));

            const auto& currentFramebuffer = finalApply ? framebufferManager.GetFramebuffer("XeGTAO/Occlusion") : workingAO;

            currentFramebuffer.image.Barrier
            (
               cmdBuffer,
               Vk::ImageBarrier{
                   .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                   .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                   .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                   .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                   .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                   .newLayout      = VK_IMAGE_LAYOUT_GENERAL,
                   .baseMipLevel   = 0,
                   .levelCount     = currentFramebuffer.image.mipLevels,
                   .baseArrayLayer = finalApply ? 0 : currentIndex,
                   .layerCount     = finalApply ? currentFramebuffer.image.arrayLayers : 1
               }
            );

            denoisePipeline.pushConstant =
            {
                .samplerIndex     = denoisePipeline.samplerIndex,
                .sourceEdgesIndex = framebufferManager.GetFramebufferView("XeGTAO/EdgesView").sampledImageIndex,
                .sourceAOIndex    = previousView.sampledImageIndex,
                .outAOIndex       = currentView.storageImageIndex,
                .finalApply       = finalApply ? 1u : 0
            };

            denoisePipeline.PushConstants
            (
                cmdBuffer,
                VK_SHADER_STAGE_COMPUTE_BIT,
                0, sizeof(Denoise::PushConstant),
                &denoisePipeline.pushConstant
            );

            vkCmdDispatch
            (
                cmdBuffer.handle,
                (workingAO.image.width  + XE_GTAO_NUM_THREADS.x - 1) / XE_GTAO_NUM_THREADS.x,
                (workingAO.image.height + XE_GTAO_NUM_THREADS.y - 1) / XE_GTAO_NUM_THREADS.y,
                1
            );

            currentFramebuffer.image.Barrier
            (
               cmdBuffer,
               Vk::ImageBarrier{
                   .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                   .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                   .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                   .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                   .oldLayout      = VK_IMAGE_LAYOUT_GENERAL,
                   .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                   .baseMipLevel   = 0,
                   .levelCount     = currentFramebuffer.image.mipLevels,
                   .baseArrayLayer = finalApply ? 0 : currentIndex,
                   .layerCount     = finalApply ? currentFramebuffer.image.arrayLayers : 1
               }
            );
        }

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::Destroy(VkDevice device)
    {
        Logger::Debug("{}\n", "Destroying XeGTAO pass!");

        depthPreFilterPipeline.Destroy(device);
        occlusionPipeline.Destroy(device);
        denoisePipeline.Destroy(device);
    }
}