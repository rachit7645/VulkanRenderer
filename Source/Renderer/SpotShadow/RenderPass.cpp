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

#include <Util/Maths.h>

#include "Renderer/RenderConstants.h"
#include "Vulkan/DebugUtils.h"
#include "Util/Log.h"

namespace Renderer::SpotShadow
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
            "SpotShadowMap",
            Vk::FramebufferType::Depth,
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferUsage::Sampled,
            Vk::FramebufferSize{
                .width       = Objects::SPOT_LIGHT_SHADOW_DIMENSIONS.x,
                .height      = Objects::SPOT_LIGHT_SHADOW_DIMENSIONS.y,
                .mipLevels   = 1,
                .arrayLayers = Objects::MAX_SHADOWED_SPOT_LIGHT_COUNT
            },
            {
                .dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        );

        framebufferManager.AddFramebufferView
        (
            "SpotShadowMap",
            "SpotShadowMapView",
            Vk::FramebufferImageType::Array2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = Objects::MAX_SHADOWED_SPOT_LIGHT_COUNT,
            }
        );

        for (usize i = 0; i < Objects::MAX_SHADOWED_SPOT_LIGHT_COUNT; ++i)
        {
            framebufferManager.AddFramebufferView
            (
                "SpotShadowMap",
                fmt::format("SpotShadowMapView/{}", i),
                Vk::FramebufferImageType::Single2D,
                Vk::FramebufferViewSize{
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = static_cast<u32>(i),
                    .layerCount     = 1,
                }
            );
        }

        Logger::Info("{}\n", "Created spot shadow pass!");
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
        if (lightsBuffer.shadowedSpotLights.empty())
        {
            return;
        }

        Vk::BeginLabel(cmdBuffer, fmt::format("SpotShadowPass/FIF{}", FIF), glm::vec4(0.2196f, 0.2418f, 0.6588f, 1.0f));

        const auto& depthAttachment = framebufferManager.GetFramebuffer("SpotShadowMap");

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

        for (usize i = 0; i < lightsBuffer.shadowedSpotLights.size(); ++i)
        {
            cullingDispatch.DispatchFrustumCulling
            (
                FIF,
                lightsBuffer.shadowedSpotLights[i].matrix,
                cmdBuffer,
                meshBuffer,
                indirectBuffer
            );

            Vk::BeginLabel(cmdBuffer, fmt::format("Light #{}", i), glm::vec4(0.5146f, 0.7488f, 0.9388f, 1.0f));

            const auto depthAttachmentView = framebufferManager.GetFramebufferView(fmt::format("SpotShadowMapView/{}", i));

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
                .currentIndex  = static_cast<u32>(i)
            };

            pipeline.PushConstants
            (
               cmdBuffer,
               VK_SHADER_STAGE_VERTEX_BIT,
               0,
               sizeof(SpotShadow::PushConstant),
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
        Logger::Debug("{}\n", "Destroying spot shadow pass!");

        pipeline.Destroy(device);
    }
}