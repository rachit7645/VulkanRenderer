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

namespace Renderer::PointShadow
{
    RenderPass::RenderPass
    (
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        Vk::FramebufferManager& framebufferManager
    )
        : pipeline(context, formatHelper)
    {
        framebufferManager.AddFramebuffer
        (
            "PointShadowMap",
            Vk::FramebufferType::Depth,
            Vk::FramebufferImageType::ArrayCube,
            Vk::FramebufferUsage::Sampled,
            Vk::FramebufferSize{
                .width       = Objects::POINT_SHADOW_DIMENSIONS.x,
                .height      = Objects::POINT_SHADOW_DIMENSIONS.y,
                .mipLevels   = 1,
                .arrayLayers = 6 * Objects::MAX_SHADOWED_POINT_LIGHT_COUNT
            },
            {
                .dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        );

        framebufferManager.AddFramebufferView
        (
            "PointShadowMap",
            "PointShadowMapView",
            Vk::FramebufferImageType::ArrayCube,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 6 * Objects::MAX_SHADOWED_POINT_LIGHT_COUNT,
            }
        );

        for (usize i = 0; i < Objects::MAX_SHADOWED_POINT_LIGHT_COUNT; ++i)
        {
            for (usize j = 0; j < 6; ++j)
            {
                framebufferManager.AddFramebufferView
                (
                    "PointShadowMap",
                    fmt::format("PointShadowMapView/Light{}/{}", i, j),
                    Vk::FramebufferImageType::Single2D,
                    Vk::FramebufferViewSize{
                        .baseMipLevel   = 0,
                        .levelCount     = 1,
                        .baseArrayLayer = static_cast<u32>(6 * i + j),
                        .layerCount     = 1,
                    }
                );
            }
        }

        Logger::Info("{}\n", "Created point shadow pass!");
    }

    void RenderPass::Render
    (
        usize FIF,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::GeometryBuffer& geometryBuffer,
        const Buffers::SceneBuffer& sceneBuffer,
        const Buffers::MeshBuffer& meshBuffer,
        const Buffers::IndirectBuffer& indirectBuffer,
        const Buffers::LightsBuffer& lightsBuffer,
        Culling::Dispatch& cullingDispatch
    )
    {
        if (lightsBuffer.shadowedPointLights.empty())
        {
            return;
        }

        Vk::BeginLabel(cmdBuffer, fmt::format("PointShadowPass/FIF{}", FIF), glm::vec4(0.4196f, 0.6488f, 0.9588f, 1.0f));

        const auto& depthAttachment = framebufferManager.GetFramebuffer("PointShadowMap");

        depthAttachment.image.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            {
                .aspectMask     = depthAttachment.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = depthAttachment.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = depthAttachment.image.arrayLayers
            }
        );

        for (usize i = 0; i < lightsBuffer.shadowedPointLights.size(); ++i)
        {
            Vk::BeginLabel(cmdBuffer, fmt::format("Light #{}", i), glm::vec4(0.7146f, 0.2488f, 0.9388f, 1.0f));

            for (usize face = 0; face < 6; ++face)
            {
                Vk::BeginLabel(cmdBuffer, fmt::format("Face #{}", face), glm::vec4(0.6146f, 0.8488f, 0.3388f, 1.0f));

                cullingDispatch.DispatchFrustumCulling
                (
                    FIF,
                    lightsBuffer.shadowedPointLights[i].matrices[face],
                    cmdBuffer,
                    meshBuffer,
                    indirectBuffer
                );

                const auto depthAttachmentView = framebufferManager.GetFramebufferView(fmt::format("PointShadowMapView/Light{}/{}", i, face));

                const VkRenderingAttachmentInfo depthAttachmentInfo =
                {
                    .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .pNext              = nullptr,
                    .imageView          = depthAttachmentView.view.handle,
                    .imageLayout        = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    .resolveMode        = VK_RESOLVE_MODE_NONE,
                    .resolveImageView   = VK_NULL_HANDLE,
                    .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .loadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
                    .clearValue         = {.depthStencil = {1.0f, 0x0}}
                };

                const VkRenderingInfo renderInfo =
                {
                    .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
                    .pNext                = nullptr,
                    .flags                = 0,
                    .renderArea           = {
                        .offset = {0, 0},
                        .extent = {depthAttachment.image.width, depthAttachment.image.height}
                    },
                    .layerCount           = 1,
                    .viewMask             = 0,
                    .colorAttachmentCount = 0,
                    .pColorAttachments    = nullptr,
                    .pDepthAttachment     = &depthAttachmentInfo,
                    .pStencilAttachment   = nullptr
                };

                vkCmdBeginRendering(cmdBuffer.handle, &renderInfo);

                pipeline.Bind(cmdBuffer);

                const VkViewport viewport =
                {
                    .x        = 0.0f,
                    .y        = 0.0f,
                    .width    = static_cast<f32>(depthAttachment.image.width),
                    .height   = static_cast<f32>(depthAttachment.image.height),
                    .minDepth = 0.0f,
                    .maxDepth = 1.0f
                };

                vkCmdSetViewportWithCount(cmdBuffer.handle, 1, &viewport);

                const VkRect2D scissor =
                {
                    .offset = {0, 0},
                    .extent = {depthAttachment.image.width, depthAttachment.image.height}
                };

                vkCmdSetScissorWithCount(cmdBuffer.handle, 1, &scissor);

                pipeline.pushConstant =
                {
                    .scene         = sceneBuffer.buffers[FIF].deviceAddress,
                    .meshes        = meshBuffer.buffers[FIF].deviceAddress,
                    .meshIndices = indirectBuffer.frustumCulledDrawCallBuffer.meshIndexBuffer.deviceAddress,
                    .positions     = geometryBuffer.positionBuffer.deviceAddress,
                    .lightIndex    = static_cast<u32>(i),
                    .faceIndex     = static_cast<u32>(face)
                };

                pipeline.PushConstants
                (
                   cmdBuffer,
                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                   0, sizeof(PointShadow::PushConstant),
                   &pipeline.pushConstant
                );

                geometryBuffer.Bind(cmdBuffer);

                vkCmdDrawIndexedIndirectCount
                (
                    cmdBuffer.handle,
                    indirectBuffer.frustumCulledDrawCallBuffer.drawCallBuffer.handle,
                    sizeof(u32),
                    indirectBuffer.frustumCulledDrawCallBuffer.drawCallBuffer.handle,
                    0,
                    indirectBuffer.drawCallBuffers[FIF].writtenDrawCount,
                    sizeof(VkDrawIndexedIndirectCommand)
                );

                vkCmdEndRendering(cmdBuffer.handle);

                Vk::EndLabel(cmdBuffer);
            }

            Vk::EndLabel(cmdBuffer);
        }

        depthAttachment.image.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            {
                .aspectMask     = depthAttachment.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = depthAttachment.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = depthAttachment.image.arrayLayers
            }
        );

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::Destroy(VkDevice device)
    {
        Logger::Debug("{}\n", "Destroying point shadow pass!");

        pipeline.Destroy(device);
    }
}
