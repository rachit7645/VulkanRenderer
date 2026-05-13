/*
 * Copyright (c) 2023 - 2026 Rachit
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
#include "Vulkan/DebugUtils.h"
#include "Deferred/GBuffer.h"

namespace Renderer::GBuffer
{
    RenderPass::RenderPass
    (
        const Vk::FormatHelper& formatHelper,
        const Vk::MegaSet& megaSet,
        Vk::PipelineManager& pipelineManager,
        Vk::FramebufferManager& framebufferManager
    )
    {
        constexpr std::array DYNAMIC_STATES = {VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT, VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT};

        constexpr std::array COLOR_FORMATS =
        {
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_R16G16_UNORM,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_B10G11R11_UFLOAT_PACK32,
            VK_FORMAT_R16G16_SFLOAT
        };

        pipelineManager.AddPipeline("GBuffer/SingleSided", Vk::PipelineConfig{}
            .SetPipelineType(VK_PIPELINE_BIND_POINT_GRAPHICS)
            .SetRenderingInfo(0, COLOR_FORMATS, formatHelper.depthFormat)
            .AttachShader("Deferred/GBuffer/GBuffer.vert",     VK_SHADER_STAGE_VERTEX_BIT)
            .AttachShader("Deferred/GBuffer/SingleSided.frag", VK_SHADER_STAGE_FRAGMENT_BIT)
            .SetDynamicStates(DYNAMIC_STATES)
            .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetRasterizerState(VK_FALSE, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL)
            .SetDepthState(VK_TRUE, VK_FALSE, VK_COMPARE_OP_EQUAL)
            .AddDefaultBlendAttachment()
            .AddDefaultBlendAttachment()
            .AddDefaultBlendAttachment()
            .AddDefaultBlendAttachment()
            .AddDefaultBlendAttachment()
            .AddPushConstant(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GBuffer::Constants))
            .AddDescriptorLayout(megaSet.layout)
        );

        pipelineManager.AddPipeline("GBuffer/DoubleSided", Vk::PipelineConfig{}
            .SetPipelineType(VK_PIPELINE_BIND_POINT_GRAPHICS)
            .SetRenderingInfo(0, COLOR_FORMATS, formatHelper.depthFormat)
            .AttachShader("Deferred/GBuffer/GBuffer.vert",     VK_SHADER_STAGE_VERTEX_BIT)
            .AttachShader("Deferred/GBuffer/DoubleSided.frag", VK_SHADER_STAGE_FRAGMENT_BIT)
            .SetDynamicStates(DYNAMIC_STATES)
            .SetInputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetRasterizerState(VK_FALSE, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL)
            .SetDepthState(VK_TRUE, VK_FALSE, VK_COMPARE_OP_EQUAL)
            .AddDefaultBlendAttachment()
            .AddDefaultBlendAttachment()
            .AddDefaultBlendAttachment()
            .AddDefaultBlendAttachment()
            .AddDefaultBlendAttachment()
            .AddPushConstant(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GBuffer::Constants))
            .AddDescriptorLayout(megaSet.layout)
        );

        framebufferManager.AddFramebuffer
        (
            "GAlbedoIoR",
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_VIEW_TYPE_2D,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            [] (const VkExtent2D& renderExtent, ENGINE_UNUSED const VkExtent2D& displayExtent) -> Vk::FramebufferSize
            {
                return
                {
                    .width       = renderExtent.width,
                    .height      = renderExtent.height,
                    .mipLevels   = 1,
                    .arrayLayers = 1
                };
            },
            Vk::FramebufferInitialState{
                .stageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .accessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .layout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        );

        framebufferManager.AddFramebuffer
        (
            "GNormal",
            VK_FORMAT_R16G16_UNORM,
            VK_IMAGE_VIEW_TYPE_2D,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            [] (const VkExtent2D& renderExtent, ENGINE_UNUSED const VkExtent2D& displayExtent) -> Vk::FramebufferSize
            {
                return
                {
                    .width       = renderExtent.width,
                    .height      = renderExtent.height,
                    .mipLevels   = 1,
                    .arrayLayers = 1
                };
            },
            Vk::FramebufferInitialState{
                .stageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .accessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .layout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        );

        framebufferManager.AddFramebuffer
        (
            "GNormalAsyncCompute",
            VK_FORMAT_R16G16_UNORM,
            VK_IMAGE_VIEW_TYPE_2D,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            [] (const VkExtent2D& renderExtent, ENGINE_UNUSED const VkExtent2D& displayExtent) -> Vk::FramebufferSize
            {
                return
                {
                    .width       = renderExtent.width,
                    .height      = renderExtent.height,
                    .mipLevels   = 1,
                    .arrayLayers = 1
                };
            },
            Vk::FramebufferInitialState{
                .stageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .accessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .layout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        );

        // Alpha channel is unused
        framebufferManager.AddFramebuffer
        (
            "GRoughnessMetallicHorizon",
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_VIEW_TYPE_2D,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            [] (const VkExtent2D& renderExtent, ENGINE_UNUSED const VkExtent2D& displayExtent) -> Vk::FramebufferSize
            {
                return
                {
                    .width       = renderExtent.width,
                    .height      = renderExtent.height,
                    .mipLevels   = 1,
                    .arrayLayers = 1
                };
            },
            Vk::FramebufferInitialState{
                .stageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .accessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .layout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        );

        framebufferManager.AddFramebuffer
        (
            "GEmmisive",
            VK_FORMAT_B10G11R11_UFLOAT_PACK32,
            VK_IMAGE_VIEW_TYPE_2D,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            [] (const VkExtent2D& renderExtent, ENGINE_UNUSED const VkExtent2D& displayExtent) -> Vk::FramebufferSize
            {
                return
                {
                    .width       = renderExtent.width,
                    .height      = renderExtent.height,
                    .mipLevels   = 1,
                    .arrayLayers = 1
                };
            },
            Vk::FramebufferInitialState{
                .stageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .accessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .layout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        );

        framebufferManager.AddFramebuffer
        (
            "GMotionVectors",
            VK_FORMAT_R16G16_SFLOAT,
            VK_IMAGE_VIEW_TYPE_2D,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            [] (const VkExtent2D& renderExtent, ENGINE_UNUSED const VkExtent2D& displayExtent) -> Vk::FramebufferSize
            {
                return
                {
                    .width       = renderExtent.width,
                    .height      = renderExtent.height,
                    .mipLevels   = 1,
                    .arrayLayers = 1
                };
            },
            Vk::FramebufferInitialState{
                .stageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .accessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .layout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        );

        framebufferManager.AddFramebufferView
        (
            "GAlbedoIoR",
            "GAlbedoIoRView",
            VK_IMAGE_VIEW_TYPE_2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        framebufferManager.AddFramebufferView
        (
            "GNormal",
            "GNormalView",
            VK_IMAGE_VIEW_TYPE_2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        framebufferManager.AddFramebufferView
        (
            "GNormalAsyncCompute",
            "GNormalAsyncComputeView",
            VK_IMAGE_VIEW_TYPE_2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        framebufferManager.AddFramebufferView
        (
            "GRoughnessMetallicHorizon",
            "GRoughnessMetallicHorizonView",
            VK_IMAGE_VIEW_TYPE_2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        framebufferManager.AddFramebufferView
        (
            "GEmmisive",
            "GEmissiveView",
            VK_IMAGE_VIEW_TYPE_2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        framebufferManager.AddFramebufferView
        (
            "GMotionVectors",
            "GMotionVectorsView",
            VK_IMAGE_VIEW_TYPE_2D,
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
        usize frameIndex,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Models::ModelManager& modelManager,
        const Buffers::SceneBuffer& sceneBuffer,
        const Buffers::MeshBuffer& meshBuffer,
        const Buffers::IndirectBuffer& indirectBuffer,
        const Objects::Samplers& samplers
    )
    {
        Vk::BeginLabel(cmdBuffer, "GBuffer Generation", glm::vec4(0.5098f, 0.1243f, 0.4549f, 1.0f));

        const auto& singleSidedPipeline = pipelineManager.GetPipeline("GBuffer/SingleSided");
        const auto& doubleSidedPipeline = pipelineManager.GetPipeline("GBuffer/DoubleSided");
        
        const auto& gAlbedoView        = framebufferManager.GetFramebufferView("GAlbedoIoRView");
        const auto& gNormalView        = framebufferManager.GetFramebufferView("GNormalView");
        const auto& gRghMtlHrzView     = framebufferManager.GetFramebufferView("GRoughnessMetallicHorizonView");
        const auto& gEmissiveView      = framebufferManager.GetFramebufferView("GEmissiveView");
        const auto& gMotionVectorsView = framebufferManager.GetFramebufferView("GMotionVectorsView");
        const auto& sceneDepthView     = framebufferManager.GetFramebufferView("SceneDepthView");

        const auto& gAlbedo        = framebufferManager.GetFramebuffer(gAlbedoView.framebuffer);
        const auto& gNormal        = framebufferManager.GetFramebuffer(gNormalView.framebuffer);
        const auto& gRghMtlHrz     = framebufferManager.GetFramebuffer(gRghMtlHrzView.framebuffer);
        const auto& gEmissive      = framebufferManager.GetFramebuffer(gEmissiveView.framebuffer);
        const auto& gMotionVectors = framebufferManager.GetFramebuffer(gMotionVectorsView.framebuffer);
        const auto& sceneDepth     = framebufferManager.GetFramebuffer(sceneDepthView.framebuffer);

        Vk::BarrierWriter barrierWriter = {};

        barrierWriter
        .WriteImageBarrier(
            gAlbedo.image,
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
                .levelCount     = gAlbedo.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = gAlbedo.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            gNormal.image,
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
                .levelCount     = gNormal.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = gNormal.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            gRghMtlHrz.image,
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
                .levelCount     = gRghMtlHrz.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = gRghMtlHrz.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            gEmissive.image,
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
                .levelCount     = gEmissive.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = gEmissive.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            gMotionVectors.image,
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
                .levelCount     = gMotionVectors.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = gMotionVectors.image.arrayLayers
            }
        )
        .Execute(cmdBuffer);

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

        const VkRenderingAttachmentInfo gRghMtlHrzInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = gRghMtlHrzView.view.handle,
            .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode        = VK_RESOLVE_MODE_NONE,
            .resolveImageView   = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue         = {{{0.0f, 0.0f, 0.0f, 0.0f}}}
        };

        const VkRenderingAttachmentInfo gEmmisiveInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = gEmissiveView.view.handle,
            .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode        = VK_RESOLVE_MODE_NONE,
            .resolveImageView   = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue         = {{{0.0f, 0.0f, 0.0f, 0.0f}}}
        };

        const VkRenderingAttachmentInfo gMotionVectorsInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = gMotionVectorsView.view.handle,
            .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode        = VK_RESOLVE_MODE_NONE,
            .resolveImageView   = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue         = {{{0.0f, 0.0f, 0.0f, 0.0f}}}
        };

        const VkRenderingAttachmentInfo sceneDepthInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = sceneDepthView.view.handle,
            .imageLayout        = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .resolveMode        = VK_RESOLVE_MODE_NONE,
            .resolveImageView   = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp             = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp            = VK_ATTACHMENT_STORE_OP_NONE,
            .clearValue         = {}
        };

        const std::array colorAttachments =
        {
            gAlbedoInfo,
            gNormalInfo,
            gRghMtlHrzInfo,
            gEmmisiveInfo,
            gMotionVectorsInfo
        };

        const VkRenderingInfo renderInfo =
        {
            .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext                = nullptr,
            .flags                = 0,
            .renderArea           = {
                .offset = {.x     = 0,                   .y      = 0},
                .extent = {.width = gAlbedo.image.width, .height = gAlbedo.image.height}
            },
            .layerCount           = 1,
            .viewMask             = 0,
            .colorAttachmentCount = colorAttachments.size(),
            .pColorAttachments    = colorAttachments.data(),
            .pDepthAttachment     = &sceneDepthInfo,
            .pStencilAttachment   = nullptr
        };

        vkCmdBeginRendering(cmdBuffer.handle, &renderInfo);

        const VkViewport viewport =
        {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<f32>(gAlbedo.image.width),
            .height   = static_cast<f32>(gAlbedo.image.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vkCmdSetViewportWithCount(cmdBuffer.handle, 1, &viewport);

        const VkRect2D scissor =
        {
            .offset = {.x     = 0,                   .y     = 0},
            .extent = {.width = gAlbedo.image.width, .height= gAlbedo.image.height}
        };

        vkCmdSetScissorWithCount(cmdBuffer.handle, 1, &scissor);

        modelManager.geometryBuffer.Bind(cmdBuffer);

        // Single Sided
        {
            Vk::BeginLabel(cmdBuffer, "Single Sided", glm::vec4(0.6091f, 0.7243f, 0.2549f, 1.0f));

            singleSidedPipeline.Bind(cmdBuffer);
            singleSidedPipeline.BindDescriptors(cmdBuffer, megaSet);

            // Opaque
            {
                Vk::BeginLabel(cmdBuffer, "Opaque", glm::vec4(0.3091f, 0.7243f, 0.2549f, 1.0f));

                const auto constants = GBuffer::Constants
                {
                    .Scene               = sceneBuffer.graphicsBuffers.sceneBuffers[FIF].deviceAddress,
                    .CurrentMeshes       = meshBuffer.GetCurrentMeshBuffer(frameIndex).deviceAddress,
                    .PreviousMeshes      = meshBuffer.GetPreviousMeshBuffer(frameIndex).deviceAddress,
                    .CurrentInstances    = meshBuffer.GetCurrentInstanceBuffer(frameIndex).deviceAddress,
                    .PreviousInstances   = meshBuffer.GetPreviousInstanceBuffer(frameIndex).deviceAddress,
                    .InstanceIndices     = indirectBuffer.frustumCulledBuffers.opaqueBuffer.instanceIndexBuffer.deviceAddress,
                    .Positions           = modelManager.geometryBuffer.GetPositionBuffer().deviceAddress,
                    .UVs                 = modelManager.geometryBuffer.GetUVBuffer().deviceAddress,
                    .Vertices            = modelManager.geometryBuffer.GetVertexBuffer().deviceAddress,
                    .TextureSamplerIndex = modelManager.textureManager.GetSampler(samplers.textureSamplerID).descriptorID
                };

                singleSidedPipeline.PushConstants
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
                    indirectBuffer.maxDrawCount,
                    sizeof(VkDrawIndexedIndirectCommand)
                );

                Vk::EndLabel(cmdBuffer);
            }

            // Alpha Masked
            {
                Vk::BeginLabel(cmdBuffer, "Alpha Masked", glm::vec4(0.6091f, 0.2213f, 0.2549f, 1.0f));

                const auto constants = GBuffer::Constants
                {
                    .Scene               = sceneBuffer.graphicsBuffers.sceneBuffers[FIF].deviceAddress,
                    .CurrentMeshes       = meshBuffer.GetCurrentMeshBuffer(frameIndex).deviceAddress,
                    .PreviousMeshes      = meshBuffer.GetPreviousMeshBuffer(frameIndex).deviceAddress,
                    .CurrentInstances    = meshBuffer.GetCurrentInstanceBuffer(frameIndex).deviceAddress,
                    .PreviousInstances   = meshBuffer.GetPreviousInstanceBuffer(frameIndex).deviceAddress,
                    .InstanceIndices     = indirectBuffer.frustumCulledBuffers.alphaMaskedBuffer.instanceIndexBuffer.deviceAddress,
                    .Positions           = modelManager.geometryBuffer.GetPositionBuffer().deviceAddress,
                    .UVs                 = modelManager.geometryBuffer.GetUVBuffer().deviceAddress,
                    .Vertices            = modelManager.geometryBuffer.GetVertexBuffer().deviceAddress,
                    .TextureSamplerIndex = modelManager.textureManager.GetSampler(samplers.textureSamplerID).descriptorID
                };

                singleSidedPipeline.PushConstants
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
                    indirectBuffer.maxDrawCount,
                    sizeof(VkDrawIndexedIndirectCommand)
                );

                Vk::EndLabel(cmdBuffer);
            }

            Vk::EndLabel(cmdBuffer);
        }

        // Double Sided
        {
            Vk::BeginLabel(cmdBuffer, "Double Sided", glm::vec4(0.9091f, 0.2243f, 0.6549f, 1.0f));

            doubleSidedPipeline.Bind(cmdBuffer);
            doubleSidedPipeline.BindDescriptors(cmdBuffer, megaSet);

            // Opaque
            {
                Vk::BeginLabel(cmdBuffer, "Opaque", glm::vec4(0.3091f, 0.7243f, 0.2549f, 1.0f));

                const auto constants = GBuffer::Constants
                {
                    .Scene               = sceneBuffer.graphicsBuffers.sceneBuffers[FIF].deviceAddress,
                    .CurrentMeshes       = meshBuffer.GetCurrentMeshBuffer(frameIndex).deviceAddress,
                    .PreviousMeshes      = meshBuffer.GetPreviousMeshBuffer(frameIndex).deviceAddress,
                    .CurrentInstances    = meshBuffer.GetCurrentInstanceBuffer(frameIndex).deviceAddress,
                    .PreviousInstances   = meshBuffer.GetPreviousInstanceBuffer(frameIndex).deviceAddress,
                    .InstanceIndices     = indirectBuffer.frustumCulledBuffers.opaqueDoubleSidedBuffer.instanceIndexBuffer.deviceAddress,
                    .Positions           = modelManager.geometryBuffer.GetPositionBuffer().deviceAddress,
                    .UVs                 = modelManager.geometryBuffer.GetUVBuffer().deviceAddress,
                    .Vertices            = modelManager.geometryBuffer.GetVertexBuffer().deviceAddress,
                    .TextureSamplerIndex = modelManager.textureManager.GetSampler(samplers.textureSamplerID).descriptorID
                };

                doubleSidedPipeline.PushConstants
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
                    indirectBuffer.maxDrawCount,
                    sizeof(VkDrawIndexedIndirectCommand)
                );

                Vk::EndLabel(cmdBuffer);
            }

            // Alpha Masked
            {
                Vk::BeginLabel(cmdBuffer, "Alpha Masked", glm::vec4(0.6091f, 0.2213f, 0.2549f, 1.0f));

                const auto constants = GBuffer::Constants
                {
                    .Scene               = sceneBuffer.graphicsBuffers.sceneBuffers[FIF].deviceAddress,
                    .CurrentMeshes       = meshBuffer.GetCurrentMeshBuffer(frameIndex).deviceAddress,
                    .PreviousMeshes      = meshBuffer.GetPreviousMeshBuffer(frameIndex).deviceAddress,
                    .CurrentInstances    = meshBuffer.GetCurrentInstanceBuffer(frameIndex).deviceAddress,
                    .PreviousInstances   = meshBuffer.GetPreviousInstanceBuffer(frameIndex).deviceAddress,
                    .InstanceIndices     = indirectBuffer.frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.instanceIndexBuffer.deviceAddress,
                    .Positions           = modelManager.geometryBuffer.GetPositionBuffer().deviceAddress,
                    .UVs                 = modelManager.geometryBuffer.GetUVBuffer().deviceAddress,
                    .Vertices            = modelManager.geometryBuffer.GetVertexBuffer().deviceAddress,
                    .TextureSamplerIndex = modelManager.textureManager.GetSampler(samplers.textureSamplerID).descriptorID
                };

                doubleSidedPipeline.PushConstants
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
                    indirectBuffer.maxDrawCount,
                    sizeof(VkDrawIndexedIndirectCommand)
                );

                Vk::EndLabel(cmdBuffer);
            }

            Vk::EndLabel(cmdBuffer);
        }

        vkCmdEndRendering(cmdBuffer.handle);

        barrierWriter
        .WriteImageBarrier(
            gAlbedo.image,
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
                .levelCount     = gAlbedo.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = gAlbedo.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            gNormal.image,
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
                .levelCount     = gNormal.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = gNormal.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            gRghMtlHrz.image,
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
                .levelCount     = gRghMtlHrz.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = gRghMtlHrz.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            gEmissive.image,
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
                .levelCount     = gEmissive.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = gEmissive.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            gMotionVectors.image,
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
                .levelCount     = gMotionVectors.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = gMotionVectors.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            sceneDepth.image,
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
                .levelCount     = sceneDepth.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = sceneDepth.image.arrayLayers
            }
        )
        .Execute(cmdBuffer);

        Vk::EndLabel(cmdBuffer);
    }
}