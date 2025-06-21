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
#include "Shadows/PointShadow/Opaque.h"
#include "Shadows/PointShadow/AlphaMasked.h"

namespace Renderer::PointShadow
{
    RenderPass::RenderPass
    (
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        Vk::FramebufferManager& framebufferManager,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager
    )
        : m_opaquePipeline(context, formatHelper),
          m_alphaMaskedPipeline(context, formatHelper, megaSet, textureManager)
    {
        framebufferManager.AddFramebuffer
        (
            "PointShadowMap",
            Vk::FramebufferType::ColorR_SFloat32,
            Vk::FramebufferImageType::ArrayCube,
            Vk::FramebufferUsage::Attachment | Vk::FramebufferUsage::Sampled,
            Vk::FramebufferSize{
                .width       = GPU::POINT_SHADOW_DIMENSIONS.x,
                .height      = GPU::POINT_SHADOW_DIMENSIONS.y,
                .mipLevels   = 1,
                .arrayLayers = 6 * GPU::MAX_SHADOWED_POINT_LIGHT_COUNT
            },
            Vk::FramebufferInitialState{
                .dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        );

        framebufferManager.AddFramebuffer
        (
            "PointShadowDepth",
            Vk::FramebufferType::Depth,
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferUsage::Attachment | Vk::FramebufferUsage::Sampled,
            Vk::FramebufferSize{
                .width       = GPU::POINT_SHADOW_DIMENSIONS.x,
                .height      = GPU::POINT_SHADOW_DIMENSIONS.y,
                .mipLevels   = 1,
                .arrayLayers = 1
            },
            Vk::FramebufferInitialState{
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
                .layerCount     = 6 * GPU::MAX_SHADOWED_POINT_LIGHT_COUNT,
            }
        );

        for (usize i = 0; i < GPU::MAX_SHADOWED_POINT_LIGHT_COUNT; ++i)
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

        framebufferManager.AddFramebufferView
        (
            "PointShadowDepth",
            "PointShadowDepthView",
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            }
        );
    }

    void RenderPass::Render
    (
        usize FIF,
        usize frameIndex,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Models::ModelManager& modelManager,
        const Buffers::SceneBuffer& sceneBuffer,
        const Buffers::MeshBuffer& meshBuffer,
        const Buffers::IndirectBuffer& indirectBuffer,
        Culling::Dispatch& culling
    ) const
    {
        if (sceneBuffer.lightsBuffer.shadowedPointLights.empty())
        {
            return;
        }

        Vk::BeginLabel(cmdBuffer, "Point Light Shadows", glm::vec4(0.4196f, 0.6488f, 0.9588f, 1.0f));

        const auto& depthView = framebufferManager.GetFramebufferView("PointShadowDepthView");

        const auto& shadowMap = framebufferManager.GetFramebuffer("PointShadowMap");
        const auto& depth     = framebufferManager.GetFramebuffer(depthView.framebuffer);

        Vk::BarrierWriter barrierWriter = {};

        barrierWriter
        .WriteImageBarrier
        (
            shadowMap.image,
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
                .levelCount     = shadowMap.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = 6 * static_cast<u32>(sceneBuffer.lightsBuffer.shadowedPointLights.size())
            }
        )
        .WriteImageBarrier
        (
            depth.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                .dstAccessMask  = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = depth.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = depth.image.arrayLayers
            }
        )
        .Execute(cmdBuffer);

        for (usize i = 0; i < sceneBuffer.lightsBuffer.shadowedPointLights.size(); ++i)
        {
            Vk::BeginLabel(cmdBuffer, fmt::format("Light #{}", i), glm::vec4(0.7146f, 0.2488f, 0.9388f, 1.0f));

            for (usize face = 0; face < 6; ++face)
            {
                Vk::BeginLabel(cmdBuffer, fmt::format("Face #{}", face), glm::vec4(0.6146f, 0.8488f, 0.3388f, 1.0f));

                culling.Frustum
                (
                    FIF,
                    frameIndex,
                    sceneBuffer.lightsBuffer.shadowedPointLights[i].matrices[face],
                    cmdBuffer,
                    meshBuffer,
                    indirectBuffer
                );

                const auto shadowMapView = framebufferManager.GetFramebufferView(fmt::format("PointShadowMapView/Light{}/{}", i, face));

                const VkRenderingAttachmentInfo colorAttachmentInfo =
                {
                    .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .pNext              = nullptr,
                    .imageView          = shadowMapView.view.handle,
                    .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .resolveMode        = VK_RESOLVE_MODE_NONE,
                    .resolveImageView   = VK_NULL_HANDLE,
                    .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .loadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
                    .clearValue         = {.color = {.float32 = {0.0f, 0.0f, 0.0f, 0.0f}}}
                };

                const VkRenderingAttachmentInfo depthAttachmentInfo =
                {
                    .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .pNext              = nullptr,
                    .imageView          = depthView.view.handle,
                    .imageLayout        = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    .resolveMode        = VK_RESOLVE_MODE_NONE,
                    .resolveImageView   = VK_NULL_HANDLE,
                    .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .loadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
                    .clearValue         = {.depthStencil = {0.0f, 0x0}}
                };

                const VkRenderingInfo renderInfo =
                {
                    .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
                    .pNext                = nullptr,
                    .flags                = 0,
                    .renderArea           = {
                        .offset = {0, 0},
                        .extent = {shadowMap.image.width, shadowMap.image.height}
                    },
                    .layerCount           = 1,
                    .viewMask             = 0,
                    .colorAttachmentCount = 1,
                    .pColorAttachments    = &colorAttachmentInfo,
                    .pDepthAttachment     = &depthAttachmentInfo,
                    .pStencilAttachment   = nullptr
                };

                vkCmdBeginRendering(cmdBuffer.handle, &renderInfo);

                const VkViewport viewport =
                {
                    .x        = 0.0f,
                    .y        = 0.0f,
                    .width    = static_cast<f32>(shadowMap.image.width),
                    .height   = static_cast<f32>(shadowMap.image.height),
                    .minDepth = 0.0f,
                    .maxDepth = 1.0f
                };

                vkCmdSetViewportWithCount(cmdBuffer.handle, 1, &viewport);

                const VkRect2D scissor =
                {
                    .offset = {0, 0},
                    .extent = {shadowMap.image.width, shadowMap.image.height}
                };

                vkCmdSetScissorWithCount(cmdBuffer.handle, 1, &scissor);

                modelManager.geometryBuffer.Bind(cmdBuffer);

                // Opaque
                {
                    Vk::BeginLabel(cmdBuffer, "Opaque", glm::vec4(0.6091f, 0.7243f, 0.2549f, 1.0f));

                    m_opaquePipeline.Bind(cmdBuffer);

                    // Single Sided
                    {
                        Vk::BeginLabel(cmdBuffer, "Single Sided", glm::vec4(0.3091f, 0.7243f, 0.2549f, 1.0f));

                        vkCmdSetCullMode(cmdBuffer.handle, VK_CULL_MODE_BACK_BIT);

                        const auto constants = Opaque::Constants
                        {
                            .Scene       = sceneBuffer.buffers[FIF].deviceAddress,
                            .Meshes      = meshBuffer.GetCurrentBuffer(frameIndex).deviceAddress,
                            .MeshIndices = indirectBuffer.frustumCulledBuffers.opaqueBuffer.meshIndexBuffer->deviceAddress,
                            .Positions   = modelManager.geometryBuffer.GetPositionBuffer().deviceAddress,
                            .LightIndex  = static_cast<u32>(i),
                            .FaceIndex   = static_cast<u32>(face)
                        };

                        m_opaquePipeline.PushConstants
                        (
                           cmdBuffer,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           constants
                        );

                        vkCmdDrawIndexedIndirectCount
                        (
                            cmdBuffer.handle,
                            indirectBuffer.frustumCulledBuffers.opaqueBuffer.drawCallBuffer.handle,
                            sizeof(u32),
                            indirectBuffer.frustumCulledBuffers.opaqueBuffer.drawCallBuffer.handle,
                            0,
                            indirectBuffer.writtenDrawCallBuffers[FIF].writtenDrawCount,
                            sizeof(VkDrawIndexedIndirectCommand)
                        );

                        Vk::EndLabel(cmdBuffer);
                    }

                    // Double Sided
                    {
                        Vk::BeginLabel(cmdBuffer, "Double Sided", glm::vec4(0.6091f, 0.2213f, 0.2549f, 1.0f));

                        vkCmdSetCullMode(cmdBuffer.handle, VK_CULL_MODE_NONE);

                        const auto constants = Opaque::Constants
                        {
                            .Scene       = sceneBuffer.buffers[FIF].deviceAddress,
                            .Meshes      = meshBuffer.GetCurrentBuffer(frameIndex).deviceAddress,
                            .MeshIndices = indirectBuffer.frustumCulledBuffers.opaqueDoubleSidedBuffer.meshIndexBuffer->deviceAddress,
                            .Positions   = modelManager.geometryBuffer.GetPositionBuffer().deviceAddress,
                            .LightIndex  = static_cast<u32>(i),
                            .FaceIndex   = static_cast<u32>(face)
                        };

                        m_opaquePipeline.PushConstants
                        (
                           cmdBuffer,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           constants
                        );

                        vkCmdDrawIndexedIndirectCount
                        (
                            cmdBuffer.handle,
                            indirectBuffer.frustumCulledBuffers.opaqueDoubleSidedBuffer.drawCallBuffer.handle,
                            sizeof(u32),
                            indirectBuffer.frustumCulledBuffers.opaqueDoubleSidedBuffer.drawCallBuffer.handle,
                            0,
                            indirectBuffer.writtenDrawCallBuffers[FIF].writtenDrawCount,
                            sizeof(VkDrawIndexedIndirectCommand)
                        );

                        Vk::EndLabel(cmdBuffer);
                    }

                    Vk::EndLabel(cmdBuffer);
                }

                // Alpha Masked
                {
                    Vk::BeginLabel(cmdBuffer, "Alpha Masked", glm::vec4(0.9091f, 0.2243f, 0.6549f, 1.0f));

                    m_alphaMaskedPipeline.Bind(cmdBuffer);

                    const std::array descriptorSets = {megaSet.descriptorSet};
                    m_alphaMaskedPipeline.BindDescriptors(cmdBuffer, 0, descriptorSets);

                    // Single Sided
                    {
                        Vk::BeginLabel(cmdBuffer, "Single Sided", glm::vec4(0.3091f, 0.7243f, 0.2549f, 1.0f));

                        vkCmdSetCullMode(cmdBuffer.handle, VK_CULL_MODE_BACK_BIT);

                        const auto constants = AlphaMasked::Constants
                        {
                            .Scene               = sceneBuffer.buffers[FIF].deviceAddress,
                            .Meshes              = meshBuffer.GetCurrentBuffer(frameIndex).deviceAddress,
                            .MeshIndices         = indirectBuffer.frustumCulledBuffers.alphaMaskedBuffer.meshIndexBuffer->deviceAddress,
                            .Positions           = modelManager.geometryBuffer.GetPositionBuffer().deviceAddress,
                            .Vertices            = modelManager.geometryBuffer.GetVertexBuffer().deviceAddress,
                            .TextureSamplerIndex = modelManager.textureManager.GetSampler(m_alphaMaskedPipeline.textureSamplerID).descriptorID,
                            .LightIndex          = static_cast<u32>(i),
                            .FaceIndex           = static_cast<u32>(face)
                        };

                        m_alphaMaskedPipeline.PushConstants
                        (
                           cmdBuffer,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           constants
                        );

                        vkCmdDrawIndexedIndirectCount
                        (
                            cmdBuffer.handle,
                            indirectBuffer.frustumCulledBuffers.alphaMaskedBuffer.drawCallBuffer.handle,
                            sizeof(u32),
                            indirectBuffer.frustumCulledBuffers.alphaMaskedBuffer.drawCallBuffer.handle,
                            0,
                            indirectBuffer.writtenDrawCallBuffers[FIF].writtenDrawCount,
                            sizeof(VkDrawIndexedIndirectCommand)
                        );

                        Vk::EndLabel(cmdBuffer);
                    }

                    // Double Sided
                    {
                        Vk::BeginLabel(cmdBuffer, "Double Sided", glm::vec4(0.6091f, 0.2213f, 0.2549f, 1.0f));

                        vkCmdSetCullMode(cmdBuffer.handle, VK_CULL_MODE_NONE);

                        const auto constants = AlphaMasked::Constants
                        {
                            .Scene               = sceneBuffer.buffers[FIF].deviceAddress,
                            .Meshes              = meshBuffer.GetCurrentBuffer(frameIndex).deviceAddress,
                            .MeshIndices         = indirectBuffer.frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.meshIndexBuffer->deviceAddress,
                            .Positions           = modelManager.geometryBuffer.GetPositionBuffer().deviceAddress,
                            .Vertices            = modelManager.geometryBuffer.GetVertexBuffer().deviceAddress,
                            .TextureSamplerIndex = modelManager.textureManager.GetSampler(m_alphaMaskedPipeline.textureSamplerID).descriptorID,
                            .LightIndex          = static_cast<u32>(i),
                            .FaceIndex           = static_cast<u32>(face)
                        };

                        m_alphaMaskedPipeline.PushConstants
                        (
                           cmdBuffer,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           constants
                        );

                        vkCmdDrawIndexedIndirectCount
                        (
                            cmdBuffer.handle,
                            indirectBuffer.frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.drawCallBuffer.handle,
                            sizeof(u32),
                            indirectBuffer.frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.drawCallBuffer.handle,
                            0,
                            indirectBuffer.writtenDrawCallBuffers[FIF].writtenDrawCount,
                            sizeof(VkDrawIndexedIndirectCommand)
                        );

                        Vk::EndLabel(cmdBuffer);
                    }

                    Vk::EndLabel(cmdBuffer);
                }

                vkCmdEndRendering(cmdBuffer.handle);

                Vk::EndLabel(cmdBuffer);
            }

            Vk::EndLabel(cmdBuffer);
        }

        barrierWriter
        .WriteImageBarrier
        (
            shadowMap.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask  = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = shadowMap.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = 6 * static_cast<u32>(sceneBuffer.lightsBuffer.shadowedPointLights.size())
            }
        )
        .WriteImageBarrier
        (
            depth.image,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                .srcAccessMask  = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = depth.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = depth.image.arrayLayers
            }
        )
        .Execute(cmdBuffer);

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::Destroy(VkDevice device)
    {
        m_opaquePipeline.Destroy(device);
        m_alphaMaskedPipeline.Destroy(device);
    }
}
