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

#include "Util/Log.h"
#include "Util/Maths.h"
#include "Util/Ranges.h"
#include "Renderer/RenderConstants.h"
#include "Renderer/Buffers/SceneBuffer.h"
#include "Vulkan/DebugUtils.h"

namespace Renderer::Lighting
{
    RenderPass::RenderPass
    (
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        Vk::FramebufferManager& framebufferManager,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager
    )
        : pipeline(context, formatHelper, megaSet, textureManager)
    {
        for (usize i = 0; i < cmdBuffers.size(); ++i)
        {
            cmdBuffers[i] = Vk::CommandBuffer
            (
                context.device,
                context.commandPool,
                VK_COMMAND_BUFFER_LEVEL_PRIMARY
            );

            Vk::SetDebugName(context.device, cmdBuffers[i].handle, fmt::format("LightingPass/FIF{}", i));
        }

        framebufferManager.AddFramebuffer
        (
            "SceneColor",
            Vk::FramebufferType::ColorHDR,
            Vk::ImageType::Single2D,
            [] (const VkExtent2D& extent, UNUSED Vk::FramebufferManager& framebufferManager) -> Vk::FramebufferSize
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
            "SceneColor",
            "SceneColorView",
            Vk::ImageType::Single2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        Logger::Info("{}\n", "Created lighting pass!");
    }

    void RenderPass::Render
    (
        usize FIF,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const IBL::IBLMaps& iblMaps,
        const Buffers::SceneBuffer& sceneBuffer,
        const Shadow::CascadeBuffer& cascadeBuffer,
        const PointShadow::PointShadowBuffer& pointShadowBuffer,
        const SpotShadow::SpotShadowBuffer& spotShadowBuffer
    )
    {
        const auto& currentCmdBuffer = cmdBuffers[FIF];

        currentCmdBuffer.Reset(0);
        currentCmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        Vk::BeginLabel(currentCmdBuffer, fmt::format("LightingPass/FIF{}", FIF), glm::vec4(0.6098f, 0.1843f, 0.7549f, 1.0f));

        const auto& colorAttachmentView = framebufferManager.GetFramebufferView("SceneColorView");
        const auto& colorAttachment     = framebufferManager.GetFramebuffer(colorAttachmentView.framebuffer);

        colorAttachment.image.Barrier
        (
            currentCmdBuffer,
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
            .clearValue         = {{{
                Renderer::CLEAR_COLOR.r,
                Renderer::CLEAR_COLOR.g,
                Renderer::CLEAR_COLOR.b,
                Renderer::CLEAR_COLOR.a
            }}}
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

        vkCmdBeginRendering(currentCmdBuffer.handle, &renderInfo);

        pipeline.Bind(currentCmdBuffer);

        const VkViewport viewport =
        {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<f32>(colorAttachment.image.width),
            .height   = static_cast<f32>(colorAttachment.image.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vkCmdSetViewportWithCount(currentCmdBuffer.handle, 1, &viewport);

        const VkRect2D scissor =
        {
            .offset = {0, 0},
            .extent = {colorAttachment.image.width, colorAttachment.image.height}
        };

        vkCmdSetScissorWithCount(currentCmdBuffer.handle, 1, &scissor);

        pipeline.pushConstant =
        {
            .scene                   = sceneBuffer.buffers[FIF].deviceAddress,
            .cascades                = cascadeBuffer.buffers[FIF].deviceAddress,
            .pointShadows            = pointShadowBuffer.buffers[FIF].deviceAddress,
            .spotShadows             = spotShadowBuffer.buffers[FIF].deviceAddress,
            .gBufferSamplerIndex     = pipeline.gBufferSamplerIndex,
            .iblSamplerIndex         = pipeline.iblSamplerIndex,
            .shadowSamplerIndex      = pipeline.shadowSamplerIndex,
            .gAlbedoIndex            = framebufferManager.GetFramebufferView("GAlbedoView").descriptorIndex,
            .gNormalIndex            = framebufferManager.GetFramebufferView("GNormal_Rgh_Mtl_View").descriptorIndex,
            .sceneDepthIndex         = framebufferManager.GetFramebufferView("SceneDepthView").descriptorIndex,
            .irradianceIndex         = iblMaps.irradianceID,
            .preFilterIndex          = iblMaps.preFilterID,
            .brdfLutIndex            = iblMaps.brdfLutID,
            .shadowMapIndex          = framebufferManager.GetFramebufferView("ShadowCascadesView").descriptorIndex,
            .pointShadowMapIndex     = framebufferManager.GetFramebufferView("PointShadowMapView").descriptorIndex,
            .spotShadowMapIndex      = framebufferManager.GetFramebufferView("SpotShadowMapView").descriptorIndex,
            .aoIndex                 = framebufferManager.GetFramebufferView("OcclusionBlurView").descriptorIndex,
        };

        pipeline.LoadPushConstants
        (
            currentCmdBuffer,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(Lighting::PushConstant),
            reinterpret_cast<void*>(&pipeline.pushConstant)
        );

        const std::array descriptorSets = {megaSet.descriptorSet.handle};
        pipeline.BindDescriptors(currentCmdBuffer, 0, descriptorSets);

        vkCmdDraw
        (
            currentCmdBuffer.handle,
            3,
            1,
            0,
            0
        );

        vkCmdEndRendering(currentCmdBuffer.handle);

        Vk::EndLabel(currentCmdBuffer);

        currentCmdBuffer.EndRecording();
    }

    void RenderPass::Destroy(VkDevice device, VkCommandPool cmdPool)
    {
        Logger::Debug("{}\n", "Destroying lighting pass!");

        for (auto&& cmdBuffer : cmdBuffers)
        {
            cmdBuffer.Free(device, cmdPool);
        }

        pipeline.Destroy(device);
    }
}
