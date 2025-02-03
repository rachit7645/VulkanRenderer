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

#include "ForwardPass.h"

#include "Renderer/SceneBuffer.h"
#include "Util/Log.h"
#include "Renderer/RenderConstants.h"
#include "Util/Maths.h"
#include "Vulkan/Util.h"
#include "Util/Ranges.h"
#include "Vulkan/DebugUtils.h"

namespace Renderer::Forward
{
    ForwardPass::ForwardPass
    (
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        Vk::FramebufferManager& framebufferManager,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager,
        VkExtent2D extent
    )
        : pipeline(context, formatHelper, megaSet, textureManager),
          m_resolution(extent)
    {
        for (usize i = 0; i < cmdBuffers.size(); ++i)
        {
            cmdBuffers[i] = Vk::CommandBuffer
            (
                context.device,
                context.commandPool,
                VK_COMMAND_BUFFER_LEVEL_PRIMARY
            );

            Vk::SetDebugName(context.device, cmdBuffers[i].handle, fmt::format("ForwardPass/FIF{}", i));
        }

        framebufferManager.AddFramebuffer(Vk::FramebufferManager::FramebufferType::ColorHDR, "ForwardColorAttachment");

        Logger::Info("{}\n", "Created forward pass!");
    }

    void ForwardPass::Recreate(VkExtent2D extent)
    {
        m_deletionQueue.FlushQueue();

        m_resolution = extent;

        Logger::Info("{}\n", "Recreated forward pass!");
    }

    void ForwardPass::Render
    (
        usize FIF,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Vk::GeometryBuffer& geometryBuffer,
        const Renderer::SceneBuffer& sceneBuffer,
        const Renderer::MeshBuffer& meshBuffer,
        const Renderer::IndirectBuffer& indirectBuffer
    )
    {
        const auto& currentCmdBuffer = cmdBuffers[FIF];

        currentCmdBuffer.Reset(0);
        currentCmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        Vk::BeginLabel(currentCmdBuffer, fmt::format("ForwardPass/FIF{}", FIF), glm::vec4(0.9098f, 0.1843f, 0.0549f, 1.0f));

        const auto& colorAttachment = framebufferManager.GetFramebuffer("ForwardColorAttachment");
        const auto& depthAttachment = framebufferManager.GetFramebuffer("DepthAttachment");

        colorAttachment.image.Barrier
        (
            currentCmdBuffer,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            {
                .aspectMask     = colorAttachment.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = colorAttachment.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        const VkRenderingAttachmentInfo colorAttachmentInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = colorAttachment.imageView.handle,
            .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode        = VK_RESOLVE_MODE_NONE,
            .resolveImageView   = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR,
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
            .imageView          = depthAttachment.imageView.handle,
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
                .extent = m_resolution
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
            .width    = static_cast<f32>(m_resolution.width),
            .height   = static_cast<f32>(m_resolution.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vkCmdSetViewportWithCount(currentCmdBuffer.handle, 1, &viewport);

        const VkRect2D scissor =
        {
            .offset = {0, 0},
            .extent = m_resolution
        };

        vkCmdSetScissorWithCount(currentCmdBuffer.handle, 1, &scissor);

        pipeline.pushConstant =
        {
            .scene        = sceneBuffer.buffers[FIF].deviceAddress,
            .meshes       = meshBuffer.buffers[FIF].deviceAddress,
            .positions    = geometryBuffer.positionBuffer.deviceAddress,
            .vertices     = geometryBuffer.vertexBuffer.deviceAddress,
            .samplerIndex = pipeline.samplerIndex
        };

        pipeline.LoadPushConstants
        (
            currentCmdBuffer,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(Forward::PushConstant),
            reinterpret_cast<void*>(&pipeline.pushConstant)
        );

        // Mega set
        std::array descriptorSets = {megaSet.descriptorSet.handle};

        pipeline.BindDescriptors
        (
            currentCmdBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            0,
            descriptorSets
        );

        geometryBuffer.Bind(currentCmdBuffer);

        vkCmdDrawIndexedIndirect
        (
            currentCmdBuffer.handle,
            indirectBuffer.buffers[FIF].handle,
            0,
            indirectBuffer.writtenDrawCount,
            sizeof(VkDrawIndexedIndirectCommand)
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
                .layerCount     = 1
            }
        );

        Vk::EndLabel(currentCmdBuffer);

        currentCmdBuffer.EndRecording();
    }

    void ForwardPass::Destroy(VkDevice device, VkCommandPool cmdPool)
    {
        Logger::Debug("{}\n", "Destroying forward pass!");

        m_deletionQueue.FlushQueue();

        for (auto&& cmdBuffer : cmdBuffers)
        {
            cmdBuffer.Free(device, cmdPool);
        }

        pipeline.Destroy(device);
    }
}