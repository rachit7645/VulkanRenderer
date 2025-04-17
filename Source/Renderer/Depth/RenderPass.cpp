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

namespace Renderer::Depth
{
    constexpr auto HI_Z_BUFFER_LEVELS = 5;

    RenderPass::RenderPass
    (
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        Vk::FramebufferManager& framebufferManager,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager
    )
        : depthPipeline(context, formatHelper),
          hiZPipeline(context, megaSet, textureManager)
    {
        framebufferManager.AddFramebuffer
        (
            "SceneDepth",
            Vk::FramebufferType::Depth,
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferUsage::Sampled | Vk::FramebufferUsage::Storage,
            [] (const VkExtent2D& extent) -> Vk::FramebufferSize
            {
                return
                {
                    .width       = extent.width,
                    .height      = extent.height,
                    .mipLevels   = HI_Z_BUFFER_LEVELS,
                    .arrayLayers = DEPTH_HISTORY_SIZE
                };
            },
            {
                .dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        );

        for (usize i = 0; i < DEPTH_HISTORY_SIZE; ++i)
        {
            framebufferManager.AddFramebufferView
            (
                "SceneDepth",
                fmt::format("SceneDepthView/{}", i),
                Vk::FramebufferImageType::Single2D,
                Vk::FramebufferViewSize{
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = static_cast<u32>(i),
                    .layerCount     = 1
                }
            );

            framebufferManager.AddFramebufferView
            (
                "SceneDepth",
                fmt::format("HiZView/{}", i),
                Vk::FramebufferImageType::Single2D,
                Vk::FramebufferViewSize{
                    .baseMipLevel   = 0,
                    .levelCount     = HI_Z_BUFFER_LEVELS,
                    .baseArrayLayer = static_cast<u32>(i),
                    .layerCount     = 1
                }
            );

            for (usize j = 1; j < HI_Z_BUFFER_LEVELS; ++j)
            {
                framebufferManager.AddFramebufferView
                (
                    "SceneDepth",
                    fmt::format("HiZView/{}/{}", i, j),
                    Vk::FramebufferImageType::Single2D,
                    Vk::FramebufferViewSize{
                        .baseMipLevel   = static_cast<u32>(j),
                        .levelCount     = 1,
                        .baseArrayLayer = static_cast<u32>(i),
                        .layerCount     = 1
                    }
                );
            }
        }

        Logger::Info("{}\n", "Created depth pass!");
    }

    void RenderPass::Render
    (
        usize FIF,
        usize frameIndex,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Vk::GeometryBuffer& geometryBuffer,
        const Buffers::SceneBuffer& sceneBuffer,
        const Buffers::MeshBuffer& meshBuffer,
        const Renderer::Scene& scene,
        const Buffers::IndirectBuffer& indirectBuffer,
        Culling::Dispatch& cullingDispatch
    )
    {
        const usize currentIndex = frameIndex % DEPTH_HISTORY_SIZE;

        const auto& sceneDepth     = framebufferManager.GetFramebuffer("SceneDepth");
        const auto& sceneDepthView = framebufferManager.GetFramebufferView(fmt::format("SceneDepthView/{}", currentIndex));

        Vk::BeginLabel(cmdBuffer, fmt::format("DepthPass/FIF{}", FIF), glm::vec4(0.2196f, 0.2588f, 0.2588f, 1.0f));

        Vk::BeginLabel(cmdBuffer, "HiZDepthRender", glm::vec4(0.2196f, 0.8588f, 0.7588f, 1.0f));

        sceneDepth.image.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            {
                .aspectMask     = sceneDepth.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = static_cast<u32>(currentIndex),
                .layerCount     = 1
            }
        );

        RenderDepth
        (
            FIF,
            cmdBuffer,
            sceneDepth,
            sceneDepthView,
            geometryBuffer,
            sceneBuffer,
            meshBuffer,
            indirectBuffer.occlusionCulledPreviouslyVisibleDrawCallBuffer,
            indirectBuffer.drawCallBuffers[FIF].writtenDrawCount,
            VK_ATTACHMENT_LOAD_OP_CLEAR
        );

        Vk::EndLabel(cmdBuffer);

        Vk::BeginLabel(cmdBuffer, "HiZDownsample", glm::vec4(0.6196f, 0.8588f, 0.7588f, 1.0f));

        hiZPipeline.Bind(cmdBuffer);

        hiZPipeline.pushConstant =
        {
            .samplerIndex      = hiZPipeline.samplerIndex,
            .depthIndex        = sceneDepthView.sampledImageIndex,
            .outDepthMip1Index = framebufferManager.GetFramebufferView(fmt::format("HiZView/{}/1", currentIndex)).storageImageIndex,
            .outDepthMip2Index = framebufferManager.GetFramebufferView(fmt::format("HiZView/{}/2", currentIndex)).storageImageIndex,
            .outDepthMip3Index = framebufferManager.GetFramebufferView(fmt::format("HiZView/{}/3", currentIndex)).storageImageIndex,
            .outDepthMip4Index = framebufferManager.GetFramebufferView(fmt::format("HiZView/{}/4", currentIndex)).storageImageIndex
        };

        hiZPipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(HiZ::PushConstant),
            &hiZPipeline.pushConstant
        );

        const std::array descriptorSets = {megaSet.descriptorSet};
        hiZPipeline.BindDescriptors(cmdBuffer, 0, descriptorSets);

        sceneDepth.image.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            {
                .aspectMask     = sceneDepth.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = static_cast<u32>(currentIndex),
                .layerCount     = 1
            }
        );

        sceneDepth.image.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_GENERAL,
            {
                .aspectMask     = sceneDepth.image.aspect,
                .baseMipLevel   = 1,
                .levelCount     = HI_Z_BUFFER_LEVELS - 1,
                .baseArrayLayer = static_cast<u32>(currentIndex),
                .layerCount     = 1
            }
        );

        vkCmdDispatch
        (
            cmdBuffer.handle,
            (sceneDepth.image.width  + 16 - 1) / 16,
            (sceneDepth.image.height + 16 - 1) / 16,
            1
        );

        sceneDepth.image.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            {
                .aspectMask     = sceneDepth.image.aspect,
                .baseMipLevel   = 1,
                .levelCount     = HI_Z_BUFFER_LEVELS - 1,
                .baseArrayLayer = static_cast<u32>(currentIndex),
                .layerCount     = 1
            }
        );

        Vk::EndLabel(cmdBuffer);

        cullingDispatch.DispatchOcclusionCulling
        (
            FIF,
            scene.currentMatrices.projection * scene.currentMatrices.view,
            cmdBuffer,
            framebufferManager,
            megaSet,
            sceneBuffer,
            meshBuffer,
            indirectBuffer
        );

        Vk::BeginLabel(cmdBuffer, "FinalDepthRender", glm::vec4(0.2196f, 0.8588f, 0.2588f, 1.0f));

        sceneDepth.image.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            {
                .aspectMask     = sceneDepth.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = static_cast<u32>(currentIndex),
                .layerCount     = 1
            }
        );

        RenderDepth
        (
            FIF,
            cmdBuffer,
            sceneDepth,
            sceneDepthView,
            geometryBuffer,
            sceneBuffer,
            meshBuffer,
            indirectBuffer.occlusionCulledNewlyVisibleDrawCallBuffer,
            indirectBuffer.drawCallBuffers[FIF].writtenDrawCount,
            VK_ATTACHMENT_LOAD_OP_LOAD
        );

        Vk::EndLabel(cmdBuffer);

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::RenderDepth
    (
        usize FIF,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::Framebuffer& framebuffer,
        const Vk::FramebufferView& framebufferView,
        const Vk::GeometryBuffer& geometryBuffer,
        const Buffers::SceneBuffer& sceneBuffer,
        const Buffers::MeshBuffer& meshBuffer,
        const Buffers::DrawCallBuffer& drawCallBuffer,
        u32 maxDrawCount,
        VkAttachmentLoadOp loadOp
    )
    {
        const VkRenderingAttachmentInfo depthAttachmentInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = framebufferView.view.handle,
            .imageLayout        = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .resolveMode        = VK_RESOLVE_MODE_NONE,
            .resolveImageView   = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp             = loadOp,
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
                .extent = {framebuffer.image.width, framebuffer.image.height}
            },
            .layerCount           = 1,
            .viewMask             = 0,
            .colorAttachmentCount = 0,
            .pColorAttachments    = nullptr,
            .pDepthAttachment     = &depthAttachmentInfo,
            .pStencilAttachment   = nullptr
        };

        vkCmdBeginRendering(cmdBuffer.handle, &renderInfo);

        depthPipeline.Bind(cmdBuffer);

        const VkViewport viewport =
        {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<f32>(framebuffer.image.width),
            .height   = static_cast<f32>(framebuffer.image.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vkCmdSetViewportWithCount(cmdBuffer.handle, 1, &viewport);

        const VkRect2D scissor =
        {
            .offset = {0, 0},
            .extent = {framebuffer.image.width, framebuffer.image.height}
        };

        vkCmdSetScissorWithCount(cmdBuffer.handle, 1, &scissor);

        depthPipeline.pushConstant =
        {
            .scene       = sceneBuffer.buffers[FIF].deviceAddress,
            .meshes      = meshBuffer.buffers[FIF].deviceAddress,
            .meshIndices = drawCallBuffer.meshIndexBuffer.deviceAddress,
            .positions   = geometryBuffer.positionBuffer.deviceAddress
        };

        depthPipeline.PushConstants
        (
           cmdBuffer,
           VK_SHADER_STAGE_VERTEX_BIT,
           0, sizeof(Depth::PushConstant),
           &depthPipeline.pushConstant
        );

        vkCmdDrawIndexedIndirectCount
        (
            cmdBuffer.handle,
            drawCallBuffer.drawCallBuffer.handle,
            sizeof(u32),
            drawCallBuffer.drawCallBuffer.handle,
            0,
            maxDrawCount,
            sizeof(VkDrawIndexedIndirectCommand)
        );

        vkCmdEndRendering(cmdBuffer.handle);
    }

    void RenderPass::Destroy(VkDevice device)
    {
        Logger::Debug("{}\n", "Destroying depth pass!");

        depthPipeline.Destroy(device);
        hiZPipeline.Destroy(device);
    }
}
