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

namespace Renderer::AO::XeGTAO
{
    // Must match the value in Constants.glsl
    constexpr u32 XE_GTAO_DEPTH_MIP_LEVELS = 5;

    RenderPass::RenderPass
    (
        const Vk::Context& context,
        Vk::FramebufferManager& framebufferManager,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager
    )
        : depthPreFilterPipeline(context, megaSet, textureManager)
    {
        for (usize i = 0; i < cmdBuffers.size(); ++i)
        {
            cmdBuffers[i] = Vk::CommandBuffer
            (
                context.device,
                context.commandPool,
                VK_COMMAND_BUFFER_LEVEL_PRIMARY
            );

            Vk::SetDebugName(context.device, cmdBuffers[i].handle, fmt::format("XeGTAOPass/FIF{}", i));
        }

        framebufferManager.AddFramebuffer
        (
            "XeGTAO/DepthMipChain",
            Vk::FramebufferType::Depth,
            Vk::ImageType::Single2D,
            true,
            [] (const VkExtent2D& extent, UNUSED Vk::FramebufferManager& framebufferManager) -> Vk::FramebufferSize
            {
                return
                {
                    .width       = extent.width,
                    .height      = extent.height,
                    .mipLevels   = XE_GTAO_DEPTH_MIP_LEVELS,
                    .arrayLayers = 1
                };
            }
        );

        framebufferManager.AddFramebufferView
        (
            "XeGTAO/DepthMipChain",
            "XeGTAO/DepthMipChainView",
            Vk::ImageType::Single2D,
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
                Vk::ImageType::Single2D,
                Vk::FramebufferViewSize{
                    .baseMipLevel   = i,
                    .levelCount     = 1,
                    .baseArrayLayer = 0,
                    .layerCount     = 1
                }
            );
        }

        Logger::Info("{}\n", "Created XeGTAO pass!");
    }

    void RenderPass::Render
    (
        usize FIF,
        usize frameIndex,
        const Renderer::Scene& scene,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet
    )
    {
        const auto& currentCmdBuffer = cmdBuffers[FIF];

        currentCmdBuffer.Reset(0);
        currentCmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        Vk::BeginLabel(currentCmdBuffer, fmt::format("XeGTAOPass/FIF{}", FIF), glm::vec4(0.9098f, 0.2843f, 0.7529f, 1.0f));

        PreFilterDepth
        (
            currentCmdBuffer,
            scene,
            framebufferManager,
            megaSet
        );

        Vk::EndLabel(currentCmdBuffer);

        currentCmdBuffer.EndRecording();
    }

    void RenderPass::PreFilterDepth
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Renderer::Scene& scene,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet
    )
    {
        Vk::BeginLabel(cmdBuffer, "DepthPreFilter", glm::vec4(0.6098f, 0.2143f, 0.4529f, 1.0f));

        depthPreFilterPipeline.Bind(cmdBuffer);

        depthPreFilterPipeline.pushConstant =
        {
            .depthSamplerIndex  = depthPreFilterPipeline.samplerIndex,
            .sceneDepthIndex    = framebufferManager.GetFramebufferView("SceneDepthView").sampledImageIndex,
            .outDepthMip0Index  = framebufferManager.GetFramebufferView("XeGTAO/DepthMipChainView/Mip0").storageImageIndex,
            .outDepthMip1Index  = framebufferManager.GetFramebufferView("XeGTAO/DepthMipChainView/Mip1").storageImageIndex,
            .outDepthMip2Index  = framebufferManager.GetFramebufferView("XeGTAO/DepthMipChainView/Mip2").storageImageIndex,
            .outDepthMip3Index  = framebufferManager.GetFramebufferView("XeGTAO/DepthMipChainView/Mip3").storageImageIndex,
            .outDepthMip4Index  = framebufferManager.GetFramebufferView("XeGTAO/DepthMipChainView/Mip4").storageImageIndex,
            .depthLinearizeMul  = -scene.currentMatrices.projection[3][2],
            .depthLinearizeAdd  = scene.currentMatrices.projection[2][2],
            .effectRadius       = m_effectRadius,
            .effectFalloffRange = m_effectFalloffRange,
            .radiusMultiplier   = m_radiusMultiplier
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

        const auto& depthMipChain = framebufferManager.GetFramebuffer("XeGTAO/DepthMipChain");

        depthMipChain.image.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_GENERAL,
            {
                .aspectMask     = depthMipChain.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = depthMipChain.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = depthMipChain.image.arrayLayers
            }
        );

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
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            {
                .aspectMask     = depthMipChain.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = depthMipChain.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = depthMipChain.image.arrayLayers
            }
        );

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::Destroy(VkDevice device, VkCommandPool cmdPool)
    {
        Logger::Debug("{}\n", "Destroying XeGTAO pass!");

        depthPreFilterPipeline.Destroy(device);

        Vk::CommandBuffer::Free(device, cmdPool, cmdBuffers);
    }
}
