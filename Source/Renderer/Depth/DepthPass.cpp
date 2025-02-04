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

#include "DepthPass.h"

#include "Vulkan/DebugUtils.h"
#include "Util/Log.h"

namespace Renderer::Depth
{
    DepthPass::DepthPass
    (
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        const Vk::MegaSet& megaSet,
        VkExtent2D extent
    )
        : pipeline(context, formatHelper, megaSet)
    {
        for (usize i = 0; i < cmdBuffers.size(); ++i)
        {
            cmdBuffers[i] = Vk::CommandBuffer
            (
                context.device,
                context.commandPool,
                VK_COMMAND_BUFFER_LEVEL_PRIMARY
            );

            Vk::SetDebugName(context.device, cmdBuffers[i].handle, fmt::format("DepthPass/FIF{}", i));
        }

        InitData(context, formatHelper, extent);

        Logger::Info("{}\n", "Created depth pass!");
    }

    void DepthPass::Recreate
    (
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        VkExtent2D extent
    )
    {
        m_deletionQueue.FlushQueue();

        InitData(context, formatHelper, extent);

        Logger::Info("{}\n", "Recreated depth pass!");
    }

    void DepthPass::InitData
    (
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        VkExtent2D extent
    )
    {
        m_resolution = extent;
        depthBuffer  = Vk::DepthBuffer(context, formatHelper, extent);

        Vk::SetDebugName(context.device, depthBuffer.depthImage.handle,     "DepthPassDepthAttachment");
        Vk::SetDebugName(context.device, depthBuffer.depthImageView.handle, "DepthPassDepthAttachment_View");

        m_deletionQueue.PushDeletor([&] ()
        {
            depthBuffer.Destroy(context.device, context.allocator);
        });
    }

    void DepthPass::Render
    (
        usize FIF,
        const Vk::GeometryBuffer& geometryBuffer,
        const Renderer::SceneBuffer& sceneBuffer,
        const Renderer::MeshBuffer& meshBuffer,
        const Renderer::IndirectBuffer& indirectBuffer
    )
    {
        const auto& currentCmdBuffer = cmdBuffers[FIF];

        currentCmdBuffer.Reset(0);
        currentCmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        Vk::BeginLabel(currentCmdBuffer, fmt::format("DepthPass/FIF{}", FIF), glm::vec4(0.2196f, 0.2588f, 0.2588f, 1.0f));

        const VkRenderingAttachmentInfo depthAttachmentInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = depthBuffer.depthImageView.handle,
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
                .extent = m_resolution
            },
            .layerCount           = 1,
            .viewMask             = 0,
            .colorAttachmentCount = 0,
            .pColorAttachments    = nullptr,
            .pDepthAttachment     = &depthAttachmentInfo,
            .pStencilAttachment   = nullptr
        };

        vkCmdBeginRendering(currentCmdBuffer.handle, &renderInfo);

        pipeline.Bind(currentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);

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
            .scene     = sceneBuffer.buffers[FIF].deviceAddress,
            .meshes    = meshBuffer.buffers[FIF].deviceAddress,
            .positions = geometryBuffer.positionBuffer.deviceAddress,
        };

        pipeline.LoadPushConstants
        (
           currentCmdBuffer,
           VK_SHADER_STAGE_VERTEX_BIT,
           0, sizeof(Depth::PushConstant),
           reinterpret_cast<void*>(&pipeline.pushConstant)
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

        Vk::EndLabel(currentCmdBuffer);

        currentCmdBuffer.EndRecording();
    }

    void DepthPass::Destroy(VkDevice device, VkCommandPool cmdPool)
    {
        Logger::Debug("{}\n", "Destroying depth pass!");

        m_deletionQueue.FlushQueue();

        Vk::CommandBuffer::Free(device, cmdPool, cmdBuffers);

        pipeline.Destroy(device);
    }
}
