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

#include "Renderer/RenderConstants.h"
#include "Vulkan/DebugUtils.h"
#include "Util/Log.h"

namespace Renderer::Skybox
{
    RenderPass::RenderPass
    (
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
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

            Vk::SetDebugName(context.device, cmdBuffers[i].handle, fmt::format("SkyboxPass/FIF{}", i));
        }

        Logger::Info("{}\n", "Created skybox pass!");
    }

    void RenderPass::Render
    (
        usize FIF,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::GeometryBuffer& geometryBuffer,
        const Buffers::SceneBuffer& sceneBuffer,
        const IBL::IBLMaps& iblMaps,
        const Vk::MegaSet& megaSet
    )
    {
        const auto& currentCmdBuffer = cmdBuffers[FIF];

        currentCmdBuffer.Reset(0);
        currentCmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        Vk::BeginLabel(currentCmdBuffer, fmt::format("SkyboxPass/FIF{}", FIF), {0.2796f, 0.8588f, 0.3548f, 1.0f});

        const auto& colorAttachmentView = framebufferManager.GetFramebufferView("SceneColorView");
        const auto& depthAttachmentView = framebufferManager.GetFramebufferView("SceneDepthView");

        const auto& colorAttachment = framebufferManager.GetFramebuffer(colorAttachmentView.framebuffer);
        const auto& depthAttachment = framebufferManager.GetFramebuffer(depthAttachmentView.framebuffer);

        const VkRenderingAttachmentInfo colorAttachmentInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = colorAttachmentView.view.handle,
            .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode        = VK_RESOLVE_MODE_NONE,
            .resolveImageView   = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp             = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue         = {{{
                Renderer::CLEAR_COLOR.r,
                Renderer::CLEAR_COLOR.g,
                Renderer::CLEAR_COLOR.b,
                Renderer::CLEAR_COLOR.a
            }}}
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
            .clearValue         = {.depthStencil = {0.0f, 0x0}}
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
            .pDepthAttachment     = &depthAttachmentInfo,
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
            .maxDepth = 0.0f
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
            .positions    = geometryBuffer.cubeBuffer.deviceAddress,
            .scene        = sceneBuffer.buffers[FIF].deviceAddress,
            .samplerIndex = pipeline.samplerIndex,
            .cubemapIndex = iblMaps.skyboxID
        };

        pipeline.LoadPushConstants
        (
           currentCmdBuffer,
           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
           0, sizeof(Skybox::PushConstant),
           reinterpret_cast<void*>(&pipeline.pushConstant)
        );

        std::array descriptorSets = {megaSet.descriptorSet.handle};
        pipeline.BindDescriptors(currentCmdBuffer, 0, descriptorSets);

        vkCmdDraw
        (
            currentCmdBuffer.handle,
            36,
            1,
            0,
            0
        );

        vkCmdEndRendering(currentCmdBuffer.handle);

        colorAttachment.image.Barrier
        (
            currentCmdBuffer,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            {
                .aspectMask     = colorAttachment.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = colorAttachment.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = colorAttachment.image.arrayLayers
            }
        );

        depthAttachment.image.Barrier
        (
            currentCmdBuffer,
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

        Vk::EndLabel(currentCmdBuffer);

        currentCmdBuffer.EndRecording();
    }

    void RenderPass::Destroy(VkDevice device, VkCommandPool cmdPool)
    {
        Logger::Debug("{}\n", "Destroying skybox pass!");

        Vk::CommandBuffer::Free(device, cmdPool, cmdBuffers);

        pipeline.Destroy(device);
    }
}
