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
#include "Renderer/Depth/RenderPass.h"
#include "Vulkan/DebugUtils.h"

namespace Renderer::GBuffer
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

            Vk::SetDebugName(context.device, cmdBuffers[i].handle, fmt::format("GBufferPass/FIF{}", i));
        }

        framebufferManager.AddFramebuffer
        (
            "GAlbedo",
            Vk::FramebufferType::ColorBGR_SFloat_10_11_11,
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferUsage::Sampled,
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

        framebufferManager.AddFramebuffer
        (
            "GNormal_Rgh_Mtl",
            Vk::FramebufferType::ColorRGBA_UNorm8,
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferUsage::Sampled,
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

        framebufferManager.AddFramebuffer
        (
            "MotionVectors",
            Vk::FramebufferType::ColorRG_SFloat,
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferUsage::Sampled,
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
            "GAlbedo",
            "GAlbedoView",
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        framebufferManager.AddFramebufferView
        (
            "GNormal_Rgh_Mtl",
            "GNormal_Rgh_Mtl_View",
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        framebufferManager.AddFramebufferView
        (
            "MotionVectors",
            "MotionVectorsView",
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        Logger::Info("{}\n", "Created GBuffer pass!");
    }

    void RenderPass::Render
    (
        usize FIF,
        usize frameIndex,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Vk::GeometryBuffer& geometryBuffer,
        const Buffers::SceneBuffer& sceneBuffer,
        const Buffers::MeshBuffer& meshBuffer,
        const Buffers::IndirectBuffer& indirectBuffer
    )
    {
        const auto& currentCmdBuffer = cmdBuffers[FIF];

        currentCmdBuffer.Reset(0);
        currentCmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        Vk::BeginLabel(currentCmdBuffer, fmt::format("GBufferPass/FIF{}", FIF), glm::vec4(0.5098f, 0.1243f, 0.4549f, 1.0f));

        const usize currentDepthIndex  = frameIndex % Depth::DEPTH_HISTORY_SIZE;
        const usize previousDepthIndex = (frameIndex + Depth::DEPTH_HISTORY_SIZE - 1) % Depth::DEPTH_HISTORY_SIZE;

        const auto& gAlbedoView         = framebufferManager.GetFramebufferView("GAlbedoView");
        const auto& gNormalView         = framebufferManager.GetFramebufferView("GNormal_Rgh_Mtl_View");
        const auto& motionVectorsView   = framebufferManager.GetFramebufferView("MotionVectorsView");
        const auto& depthAttachmentView = framebufferManager.GetFramebufferView(fmt::format("SceneDepthView/{}", currentDepthIndex));

        const auto& gAlbedo         = framebufferManager.GetFramebuffer(gAlbedoView.framebuffer);
        const auto& gNormal         = framebufferManager.GetFramebuffer(gNormalView.framebuffer);
        const auto& motionVectors   = framebufferManager.GetFramebuffer(motionVectorsView.framebuffer);

        gAlbedo.image.Barrier
        (
            currentCmdBuffer,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            {
                .aspectMask     = gAlbedo.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = gAlbedo.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = gAlbedo.image.arrayLayers
            }
        );

        gNormal.image.Barrier
        (
            currentCmdBuffer,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            {
                .aspectMask     = gNormal.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = gNormal.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = gNormal.image.arrayLayers
            }
        );

        motionVectors.image.Barrier
        (
            currentCmdBuffer,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            {
                .aspectMask     = motionVectors.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = motionVectors.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = motionVectors.image.arrayLayers
            }
        );

        const VkRenderingAttachmentInfo gAlbedoInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = gAlbedoView.view.handle,
            .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode        = VK_RESOLVE_MODE_NONE,
            .resolveImageView   = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue         = {{{0.0f, 0.0f, 0.0f, 0.0f}}}
        };

        const VkRenderingAttachmentInfo gNormalInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = gNormalView.view.handle,
            .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode        = VK_RESOLVE_MODE_NONE,
            .resolveImageView   = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue         = {{{0.0f, 0.0f, 0.0f, 0.0f}}}
        };

        const VkRenderingAttachmentInfo motionVectorsInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = motionVectorsView.view.handle,
            .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode        = VK_RESOLVE_MODE_NONE,
            .resolveImageView   = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue         = {{{0.0f, 0.0f, 0.0f, 0.0f}}}
        };

        const VkRenderingAttachmentInfo depthAttachmentInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = depthAttachmentView.view.handle,
            .imageLayout        = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .resolveMode        = VK_RESOLVE_MODE_NONE,
            .resolveImageView   = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp             = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp            = VK_ATTACHMENT_STORE_OP_NONE,
            .clearValue         = {}
        };

        const std::array colorAttachments = {gAlbedoInfo, gNormalInfo, motionVectorsInfo};

        const VkRenderingInfo renderInfo =
        {
            .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext                = nullptr,
            .flags                = 0,
            .renderArea           = {
                .offset = {0, 0},
                .extent = {gAlbedo.image.width, gAlbedo.image.height}
            },
            .layerCount           = 1,
            .viewMask             = 0,
            .colorAttachmentCount = colorAttachments.size(),
            .pColorAttachments    = colorAttachments.data(),
            .pDepthAttachment     = &depthAttachmentInfo,
            .pStencilAttachment   = nullptr
        };

        vkCmdBeginRendering(currentCmdBuffer.handle, &renderInfo);

        pipeline.Bind(currentCmdBuffer);

        const VkViewport viewport =
        {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<f32>(gAlbedo.image.width),
            .height   = static_cast<f32>(gAlbedo.image.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vkCmdSetViewportWithCount(currentCmdBuffer.handle, 1, &viewport);

        const VkRect2D scissor =
        {
            .offset = {0, 0},
            .extent = {gAlbedo.image.width, gAlbedo.image.height}
        };

        vkCmdSetScissorWithCount(currentCmdBuffer.handle, 1, &scissor);

        pipeline.pushConstant =
        {
            .scene               = sceneBuffer.buffers[FIF].deviceAddress,
            .meshes              = meshBuffer.meshBuffers[FIF].deviceAddress,
            .visibleMeshes       = meshBuffer.visibilityBuffer.deviceAddress,
            .positions           = geometryBuffer.positionBuffer.deviceAddress,
            .vertices            = geometryBuffer.vertexBuffer.deviceAddress,
            .textureSamplerIndex = pipeline.textureSamplerIndex,
            .depthSamplerIndex   = pipeline.depthSamplerIndex,
            .previousDepthIndex  = framebufferManager.GetFramebufferView(fmt::format("SceneDepthView/{}", previousDepthIndex)).sampledImageIndex
        };

        pipeline.PushConstants
        (
            currentCmdBuffer,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(GBuffer::PushConstant),
            &pipeline.pushConstant
        );

        const std::array descriptorSets = {megaSet.descriptorSet};
        pipeline.BindDescriptors(currentCmdBuffer, 0, descriptorSets);

        geometryBuffer.Bind(currentCmdBuffer);

        vkCmdDrawIndexedIndirectCount
        (
            currentCmdBuffer.handle,
            indirectBuffer.culledDrawCallBuffer.handle,
            sizeof(u32),
            indirectBuffer.culledDrawCallBuffer.handle,
            0,
            indirectBuffer.writtenDrawCount,
            sizeof(VkDrawIndexedIndirectCommand)
        );

        vkCmdEndRendering(currentCmdBuffer.handle);

        gAlbedo.image.Barrier
        (
            currentCmdBuffer,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            {
                .aspectMask     = gAlbedo.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = gAlbedo.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = gAlbedo.image.arrayLayers
            }
        );

        gNormal.image.Barrier
        (
            currentCmdBuffer,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            {
                .aspectMask     = gNormal.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = gNormal.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = gNormal.image.arrayLayers
            }
        );

        motionVectors.image.Barrier
        (
            currentCmdBuffer,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            {
                .aspectMask     = motionVectors.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = motionVectors.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = motionVectors.image.arrayLayers
            }
        );

        Vk::EndLabel(currentCmdBuffer);

        currentCmdBuffer.EndRecording();
    }

    void RenderPass::Destroy(VkDevice device, VkCommandPool cmdPool)
    {
        Logger::Debug("{}\n", "Destroying GBuffer pass!");

        for (auto&& cmdBuffer : cmdBuffers)
        {
            cmdBuffer.Free(device, cmdPool);
        }

        pipeline.Destroy(device);
    }
}
