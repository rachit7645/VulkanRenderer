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
#include "Util/Ranges.h"
#include "Renderer/Buffers/SceneBuffer.h"
#include "Renderer/Depth/RenderPass.h"
#include "Vulkan/DebugUtils.h"
#include "Deferred/Lighting.h"

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
        : m_pipeline(context, formatHelper, megaSet, textureManager)
    {
        framebufferManager.AddFramebuffer
        (
            "SceneColor",
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
            {
                .dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        );

        framebufferManager.AddFramebufferView
        (
            "SceneColor",
            "SceneColorView",
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
        usize FIF,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Vk::TextureManager& textureManager,
        const Buffers::SceneBuffer& sceneBuffer,
        const IBL::IBLMaps& iblMaps
    )
    {
        Vk::BeginLabel(cmdBuffer, "Lighting", glm::vec4(0.6098f, 0.1843f, 0.7549f, 1.0f));

        const auto& colorAttachmentView = framebufferManager.GetFramebufferView("SceneColorView");
        const auto& colorAttachment     = framebufferManager.GetFramebuffer(colorAttachmentView.framebuffer);

        colorAttachment.image.Barrier
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
            .clearValue         = {}
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

        vkCmdBeginRendering(cmdBuffer.handle, &renderInfo);

        m_pipeline.Bind(cmdBuffer);

        const VkViewport viewport =
        {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<f32>(colorAttachment.image.width),
            .height   = static_cast<f32>(colorAttachment.image.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vkCmdSetViewportWithCount(cmdBuffer.handle, 1, &viewport);

        const VkRect2D scissor =
        {
            .offset = {0, 0},
            .extent = {colorAttachment.image.width, colorAttachment.image.height}
        };

        vkCmdSetScissorWithCount(cmdBuffer.handle, 1, &scissor);

        const auto constants = Lighting::Constants
        {
            .Scene               = sceneBuffer.buffers[FIF].deviceAddress,
            .GBufferSamplerIndex = textureManager.GetSampler(m_pipeline.gBufferSamplerID).descriptorID,
            .IBLSamplerIndex     = textureManager.GetSampler(m_pipeline.iblSamplerID).descriptorID,
            .ShadowSamplerIndex  = textureManager.GetSampler(m_pipeline.shadowSamplerID).descriptorID,
            .GAlbedoIndex        = framebufferManager.GetFramebufferView("GAlbedoReflectanceView").sampledImageID,
            .GNormalIndex        = framebufferManager.GetFramebufferView("GNormalView").sampledImageID,
            .GRghMtlIndex        = framebufferManager.GetFramebufferView("GRoughnessMetallicView").sampledImageID,
            .GEmmisiveIndex      = framebufferManager.GetFramebufferView("GEmmisiveView").sampledImageID,
            .SceneDepthIndex     = framebufferManager.GetFramebufferView("SceneDepthView").sampledImageID,
            .IrradianceIndex     = textureManager.GetTexture(iblMaps.irradianceMapID).descriptorID,
            .PreFilterIndex      = textureManager.GetTexture(iblMaps.preFilterMapID).descriptorID,
            .BRDFLUTIndex        = textureManager.GetTexture(iblMaps.brdfLutID).descriptorID,
            .ShadowMapIndex      = framebufferManager.GetFramebufferView("ShadowRTView").sampledImageID,
            .PointShadowMapIndex = framebufferManager.GetFramebufferView("PointShadowMapView").sampledImageID,
            .AOIndex             = framebufferManager.GetFramebufferView("VBGTAO/OcclusionView").sampledImageID
        };

        m_pipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            constants
        );

        const std::array descriptorSets = {megaSet.descriptorSet};
        m_pipeline.BindDescriptors(cmdBuffer, 0, descriptorSets);

        vkCmdDraw
        (
            cmdBuffer.handle,
            3,
            1,
            0,
            0
        );

        vkCmdEndRendering(cmdBuffer.handle);

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::Destroy(VkDevice device)
    {
        m_pipeline.Destroy(device);
    }
}
