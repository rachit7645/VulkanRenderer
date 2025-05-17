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
#include "Deferred/Depth.h"

namespace Renderer::Depth
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
            "SceneDepth",
            Vk::FramebufferType::Depth,
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
            "SceneDepth",
            "SceneDepthView",
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        Logger::Info("{}\n", "Created depth pass!");
    }

    void RenderPass::Render
    (
        usize FIF,
        usize frameIndex,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::GeometryBuffer& geometryBuffer,
        const Buffers::SceneBuffer& sceneBuffer,
        const Buffers::MeshBuffer& meshBuffer,
        const Buffers::IndirectBuffer& indirectBuffer,
        Culling::Dispatch& cullingDispatch
    )
    {
        const auto& currentMatrices = sceneBuffer.gpuScene.currentMatrices;
        const auto  projectionView  = currentMatrices.projection * currentMatrices.view;

        cullingDispatch.DispatchFrustumCulling
        (
            FIF,
            frameIndex,
            projectionView,
            cmdBuffer,
            meshBuffer,
            indirectBuffer
        );

        Vk::BeginLabel(cmdBuffer, "DepthPass", glm::vec4(0.2196f, 0.2588f, 0.2588f, 1.0f));

        const auto& depthAttachmentView = framebufferManager.GetFramebufferView("SceneDepthView");
        const auto& depthAttachment     = framebufferManager.GetFramebuffer(depthAttachmentView.framebuffer);

        depthAttachment.image.Barrier
        (
            cmdBuffer,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                .dstAccessMask  = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                .baseMipLevel   = 0,
                .levelCount     = depthAttachment.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = depthAttachment.image.arrayLayers
            }
        );

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
            .clearValue         = {.depthStencil = {0.0f, 0x0}}
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

        geometryBuffer.Bind(cmdBuffer);

        // Opaque, Single Sided
        {
            vkCmdSetCullMode(cmdBuffer.handle, VK_CULL_MODE_BACK_BIT);

            const auto constants = Depth::Constants
            {
                .Scene       = sceneBuffer.buffers[FIF].deviceAddress,
                .Meshes      = meshBuffer.GetCurrentBuffer(frameIndex).deviceAddress,
                .MeshIndices = indirectBuffer.frustumCulledOpaqueBuffer.meshIndexBuffer->deviceAddress,
                .Positions   = geometryBuffer.positionBuffer.buffer.deviceAddress
            };

            pipeline.PushConstants
            (
               cmdBuffer,
               VK_SHADER_STAGE_VERTEX_BIT,
               constants
            );

            vkCmdDrawIndexedIndirectCount
            (
                cmdBuffer.handle,
                indirectBuffer.frustumCulledOpaqueBuffer.drawCallBuffer.handle,
                sizeof(u32),
                indirectBuffer.frustumCulledOpaqueBuffer.drawCallBuffer.handle,
                0,
                indirectBuffer.writtenDrawCallBuffers[FIF].writtenDrawCount,
                sizeof(VkDrawIndexedIndirectCommand)
            );
        }

        // Opaque, Double Sided
        {
            vkCmdSetCullMode(cmdBuffer.handle, VK_CULL_MODE_NONE);

            const auto constants = Depth::Constants
            {
                .Scene       = sceneBuffer.buffers[FIF].deviceAddress,
                .Meshes      = meshBuffer.GetCurrentBuffer(frameIndex).deviceAddress,
                .MeshIndices = indirectBuffer.frustumCulledOpaqueDoubleSidedBuffer.meshIndexBuffer->deviceAddress,
                .Positions   = geometryBuffer.positionBuffer.buffer.deviceAddress
            };

            pipeline.PushConstants
            (
               cmdBuffer,
               VK_SHADER_STAGE_VERTEX_BIT,
               constants
            );

            vkCmdDrawIndexedIndirectCount
            (
                cmdBuffer.handle,
                indirectBuffer.frustumCulledOpaqueDoubleSidedBuffer.drawCallBuffer.handle,
                sizeof(u32),
                indirectBuffer.frustumCulledOpaqueDoubleSidedBuffer.drawCallBuffer.handle,
                0,
                indirectBuffer.writtenDrawCallBuffers[FIF].writtenDrawCount,
                sizeof(VkDrawIndexedIndirectCommand)
            );
        }

        vkCmdEndRendering(cmdBuffer.handle);

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::Destroy(VkDevice device)
    {
        Logger::Debug("{}\n", "Destroying depth pass!");

        pipeline.Destroy(device);
    }
}
