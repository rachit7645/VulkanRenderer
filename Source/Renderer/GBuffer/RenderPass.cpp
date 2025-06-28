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
            VK_FORMAT_R8G8_UNORM,
            VK_FORMAT_B10G11R11_UFLOAT_PACK32,
            VK_FORMAT_R16G16_SFLOAT
        };

        pipelineManager.AddPipeline("GBuffer/SingleSided", Vk::PipelineConfig{}
            .SetPipelineType(VK_PIPELINE_BIND_POINT_GRAPHICS)
            .SetRenderingInfo(0, COLOR_FORMATS, formatHelper.depthFormat)
            .AttachShader("Deferred/GBuffer/GBuffer.vert",     VK_SHADER_STAGE_VERTEX_BIT)
            .AttachShader("Deferred/GBuffer/SingleSided.frag", VK_SHADER_STAGE_FRAGMENT_BIT)
            .SetDynamicStates(DYNAMIC_STATES)
            .SetIAState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetRasterizerState(VK_FALSE, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL)
            .SetDepthStencilState(VK_TRUE, VK_FALSE, VK_COMPARE_OP_EQUAL)
            .AddDefaultBlendAttachment()
            .AddDefaultBlendAttachment()
            .AddDefaultBlendAttachment()
            .AddDefaultBlendAttachment()
            .AddDefaultBlendAttachment()
            .AddPushConstant(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GBuffer::Constants))
            .AddDescriptorLayout(megaSet.descriptorLayout)
        );

        pipelineManager.AddPipeline("GBuffer/DoubleSided", Vk::PipelineConfig{}
            .SetPipelineType(VK_PIPELINE_BIND_POINT_GRAPHICS)
            .SetRenderingInfo(0, COLOR_FORMATS, formatHelper.depthFormat)
            .AttachShader("Deferred/GBuffer/GBuffer.vert",     VK_SHADER_STAGE_VERTEX_BIT)
            .AttachShader("Deferred/GBuffer/DoubleSided.frag", VK_SHADER_STAGE_FRAGMENT_BIT)
            .SetDynamicStates(DYNAMIC_STATES)
            .SetIAState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetRasterizerState(VK_FALSE, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL)
            .SetDepthStencilState(VK_TRUE, VK_FALSE, VK_COMPARE_OP_EQUAL)
            .AddDefaultBlendAttachment()
            .AddDefaultBlendAttachment()
            .AddDefaultBlendAttachment()
            .AddDefaultBlendAttachment()
            .AddDefaultBlendAttachment()
            .AddPushConstant(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GBuffer::Constants))
            .AddDescriptorLayout(megaSet.descriptorLayout)
        );

        framebufferManager.AddFramebuffer
        (
            "GAlbedoReflectance",
            Vk::FramebufferType::ColorRGBA_UNorm8,
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
            Vk::FramebufferInitialState{
                .dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        );

        framebufferManager.AddFramebuffer
        (
            "GNormal",
            Vk::FramebufferType::ColorRG_Unorm16,
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferUsage::Attachment | Vk::FramebufferUsage::Sampled | Vk::FramebufferUsage::TransferSource,
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
            Vk::FramebufferInitialState{
                .dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        );

        framebufferManager.AddFramebuffer
        (
            "GNormalAsyncCompute",
            Vk::FramebufferType::ColorRG_Unorm16,
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferUsage::Sampled | Vk::FramebufferUsage::TransferDestination,
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
            Vk::FramebufferInitialState{
                .dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        );

        framebufferManager.AddFramebuffer
        (
            "GRoughnessMetallic",
            Vk::FramebufferType::ColorRG_Unorm8,
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
            Vk::FramebufferInitialState{
                .dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        );

        framebufferManager.AddFramebuffer
        (
            "GEmmisive",
            Vk::FramebufferType::ColorBGR_SFloat_10_11_11,
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
            Vk::FramebufferInitialState{
                .dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        );

        framebufferManager.AddFramebuffer
        (
            "GMotionVectors",
            Vk::FramebufferType::ColorRG_SFloat16,
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
            Vk::FramebufferInitialState{
                .dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        );

        framebufferManager.AddFramebufferView
        (
            "GAlbedoReflectance",
            "GAlbedoReflectanceView",
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
            "GNormal",
            "GNormalView",
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
            "GNormalAsyncCompute",
            "GNormalAsyncComputeView",
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
            "GRoughnessMetallic",
            "GRoughnessMetallicView",
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
            "GEmmisive",
            "GEmmisiveView",
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
            "GMotionVectors",
            "GMotionVectorsView",
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
        usize frameIndex,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Models::ModelManager& modelManager,
        const Buffers::SceneBuffer& sceneBuffer,
        const Buffers::MeshBuffer& meshBuffer,
        const Buffers::IndirectBuffer& indirectBuffer,
        const Objects::GlobalSamplers& samplers
    )
    {
        Vk::BeginLabel(cmdBuffer, "GBuffer Generation", glm::vec4(0.5098f, 0.1243f, 0.4549f, 1.0f));

        const auto& singleSidedPipeline = pipelineManager.GetPipeline("GBuffer/SingleSided");
        const auto& doubleSidedPipeline = pipelineManager.GetPipeline("GBuffer/DoubleSided");
        
        const auto& gAlbedoView        = framebufferManager.GetFramebufferView("GAlbedoReflectanceView");
        const auto& gNormalView        = framebufferManager.GetFramebufferView("GNormalView");
        const auto& gRghMtlView        = framebufferManager.GetFramebufferView("GRoughnessMetallicView");
        const auto& gEmmisiveView      = framebufferManager.GetFramebufferView("GEmmisiveView");
        const auto& gMotionVectorsView = framebufferManager.GetFramebufferView("GMotionVectorsView");
        const auto& sceneDepthView     = framebufferManager.GetFramebufferView("SceneDepthView");

        const auto& gAlbedo        = framebufferManager.GetFramebuffer(gAlbedoView.framebuffer);
        const auto& gNormal        = framebufferManager.GetFramebuffer(gNormalView.framebuffer);
        const auto& gRghMtl        = framebufferManager.GetFramebuffer(gRghMtlView.framebuffer);
        const auto& gEmmisive      = framebufferManager.GetFramebuffer(gEmmisiveView.framebuffer);
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
            gRghMtl.image,
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
                .levelCount     = gRghMtl.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = gRghMtl.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            gEmmisive.image,
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
                .levelCount     = gEmmisive.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = gEmmisive.image.arrayLayers
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

        const VkRenderingAttachmentInfo gRghMtlInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = gRghMtlView.view.handle,
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
            .imageView          = gEmmisiveView.view.handle,
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
            gRghMtlInfo,
            gEmmisiveInfo,
            gMotionVectorsInfo
        };

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
            .offset = {0, 0},
            .extent = {gAlbedo.image.width, gAlbedo.image.height}
        };

        vkCmdSetScissorWithCount(cmdBuffer.handle, 1, &scissor);

        modelManager.geometryBuffer.Bind(cmdBuffer);

        // Single Sided
        {
            Vk::BeginLabel(cmdBuffer, "Single Sided", glm::vec4(0.6091f, 0.7243f, 0.2549f, 1.0f));

            singleSidedPipeline.Bind(cmdBuffer);

            const std::array descriptorSets = {megaSet.descriptorSet};
            singleSidedPipeline.BindDescriptors(cmdBuffer, 0, descriptorSets);

            // Opaque
            {
                Vk::BeginLabel(cmdBuffer, "Opaque", glm::vec4(0.3091f, 0.7243f, 0.2549f, 1.0f));

                const auto constants = GBuffer::Constants
                {
                    .Scene               = sceneBuffer.buffers[FIF].deviceAddress,
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
                    .Scene               = sceneBuffer.buffers[FIF].deviceAddress,
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

            const std::array descriptorSets = {megaSet.descriptorSet};
            doubleSidedPipeline.BindDescriptors(cmdBuffer, 0, descriptorSets);

            // Opaque
            {
                Vk::BeginLabel(cmdBuffer, "Opaque", glm::vec4(0.3091f, 0.7243f, 0.2549f, 1.0f));

                const auto constants = GBuffer::Constants
                {
                    .Scene               = sceneBuffer.buffers[FIF].deviceAddress,
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
                    .Scene               = sceneBuffer.buffers[FIF].deviceAddress,
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
            gRghMtl.image,
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
                .levelCount     = gRghMtl.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = gRghMtl.image.arrayLayers
            }
        )
        .WriteImageBarrier(
            gEmmisive.image,
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
                .levelCount     = gEmmisive.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = gEmmisive.image.arrayLayers
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