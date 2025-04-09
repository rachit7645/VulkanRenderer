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

namespace Renderer::DearImGui
{
    RenderPass::RenderPass
    (
        const Vk::Context& context,
        const Vk::Swapchain& swapchain,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager
    )
        : pipeline(context, megaSet, textureManager, swapchain.imageFormat)
    {
        Logger::Info("{}\n", "Created imgui pass!");
    }

    void RenderPass::Render
    (
        usize FIF,
        Vk::CommandBufferAllocator& cmdBufferAllocator,
        const Vk::Context& context,
        const Vk::Swapchain& swapchain,
        const Vk::MegaSet& megaSet
    )
    {
        m_deletionQueues[FIF].FlushQueue();

        ImGui::Render();
        const auto drawData = ImGui::GetDrawData();

        const auto cmdBuffer = cmdBufferAllocator.AllocateCommandBuffer(FIF, context.device, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        cmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        Vk::BeginLabel(cmdBuffer, fmt::format("ImGuiPass/FIF{}", FIF), glm::vec4(0.9137f, 0.4745f, 0.9882f, 1.0f));

        if (drawData->TotalVtxCount > 0)
        {
            RenderGui
            (
                FIF,
                drawData,
                cmdBuffer,
                context,
                swapchain,
                megaSet
            );
        }

        auto& currentImage = swapchain.images[swapchain.imageIndex];

        currentImage.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
            VK_ACCESS_2_NONE,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            {
                .aspectMask     = currentImage.aspect,
                .baseMipLevel   = 0,
                .levelCount     = currentImage.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        Vk::EndLabel(cmdBuffer);

        cmdBuffer.EndRecording();
    }

    void RenderPass::RenderGui
    (
        usize FIF,
        const ImDrawData* drawData,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::Context& context,
        const Vk::Swapchain& swapchain,
        const Vk::MegaSet& megaSet
    )
    {
        const auto displaySize      = glm::vec2(drawData->DisplaySize.x,      drawData->DisplaySize.y);
        const auto displayPos       = glm::vec2(drawData->DisplayPos.x,       drawData->DisplayPos.y);
        const auto framebufferScale = glm::vec2(drawData->FramebufferScale.x, drawData->FramebufferScale.y);

        const auto resolution = displaySize * framebufferScale;

        auto& currentVertexBuffer = m_vertexBuffers[FIF];
        auto& currentIndexBuffer  = m_indexBuffers[FIF];

        UploadToBuffers
        (
            FIF,
            drawData,
            cmdBuffer,
            context,
            currentVertexBuffer,
            currentIndexBuffer
        );

        const auto& currentImageView = swapchain.imageViews[swapchain.imageIndex];

        const VkRenderingAttachmentInfo colorAttachmentInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = currentImageView.handle,
            .imageLayout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode        = VK_RESOLVE_MODE_NONE,
            .resolveImageView   = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp             = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue         = {}
        };

        const VkRenderingInfo renderInfo =
        {
            .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext                = nullptr,
            .flags                = 0,
            .renderArea           = {
                .offset = {0, 0},
                .extent = swapchain.extent
            },
            .layerCount           = 1,
            .viewMask             = 0,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &colorAttachmentInfo,
            .pDepthAttachment     = nullptr,
            .pStencilAttachment   = nullptr
        };

        vkCmdBeginRendering(cmdBuffer.handle, &renderInfo);

        pipeline.Bind(cmdBuffer);

        vkCmdBindIndexBuffer
        (
            cmdBuffer.handle,
            currentIndexBuffer.handle,
            0,
            sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32
        );

        const VkViewport viewport =
        {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = resolution.x,
            .height   = resolution.y,
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vkCmdSetViewportWithCount(cmdBuffer.handle, 1, &viewport);

        pipeline.pushConstant.vertices     = currentVertexBuffer.deviceAddress;
        pipeline.pushConstant.scale        = glm::vec2(2.0f) / displaySize;
        pipeline.pushConstant.translate    = glm::vec2(-1.0f) - (displayPos * pipeline.pushConstant.scale);
        pipeline.pushConstant.samplerIndex = pipeline.samplerIndex;

        pipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            offsetof(PushConstant, textureIndex),
            &pipeline.pushConstant
        );

        std::array descriptorSets = {megaSet.descriptorSet};
        pipeline.BindDescriptors(cmdBuffer, 0, descriptorSets);

        s32 globalVertexOffset = 0;
        s32 globalIndexOffset  = 0;

        for (const auto drawList : drawData->CmdLists)
        {
            for (const auto& cmd : drawList->CmdBuffer)
            {
                auto clipMin = (glm::vec2(cmd.ClipRect.x, cmd.ClipRect.y) - displayPos) * framebufferScale;
                auto clipMax = (glm::vec2(cmd.ClipRect.z, cmd.ClipRect.w) - displayPos) * framebufferScale;

                if (clipMin.x < 0.0f) { clipMin.x = 0.0f; }
                if (clipMin.y < 0.0f) { clipMin.y = 0.0f; }

                if (clipMax.x > resolution.x) { clipMax.x = resolution.x; }
                if (clipMax.y > resolution.y) { clipMax.y = resolution.y; }

                if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                {
                    continue;
                }

                const VkRect2D scissor =
                {
                    .offset = {static_cast<s32>(clipMin.x), static_cast<s32>(clipMin.y)},
                    .extent = {static_cast<u32>(clipMax.x - clipMin.x), static_cast<u32>(clipMax.y - clipMin.y)}
                };

                vkCmdSetScissorWithCount(cmdBuffer.handle, 1, &scissor);

                pipeline.pushConstant.textureIndex = cmd.GetTexID();
                pipeline.PushConstants
                (
                    cmdBuffer,
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    offsetof(PushConstant, textureIndex),
                    sizeof(u32),
                    &pipeline.pushConstant.textureIndex
                );

                vkCmdDrawIndexed
                (
                    cmdBuffer.handle,
                    cmd.ElemCount,
                    1,
                    cmd.IdxOffset + globalIndexOffset,
                    static_cast<s32>(cmd.VtxOffset) + globalVertexOffset,
                    0
                );
            }

            globalVertexOffset += drawList->VtxBuffer.Size;
            globalIndexOffset  += drawList->IdxBuffer.Size;
        }

        vkCmdEndRendering(cmdBuffer.handle);
    }

    void RenderPass::UploadToBuffers
    (
        usize FIF,
        const ImDrawData* drawData,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::Context& context,
        Vk::Buffer& vertexBuffer,
        Vk::Buffer& indexBuffer
    )
    {
        // Create or resize the vertex/index buffers
        const VkDeviceSize vertexSize = drawData->TotalVtxCount * sizeof(ImDrawVert);
        const VkDeviceSize indexSize  = drawData->TotalIdxCount * sizeof(ImDrawIdx);

        if (vertexBuffer.requestedSize < vertexSize)
        {
            Vk::SetDebugName(context.device, vertexBuffer.handle, fmt::format("ImGuiPass/Deleted/VertexBuffer/{}", FIF));

            m_deletionQueues[FIF].PushDeletor([allocator = context.allocator, vertexBuffer] () mutable
            {
                vertexBuffer.Destroy(allocator);
            });

            vertexBuffer = Vk::Buffer
            (
                context.allocator,
                vertexSize,
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            vertexBuffer.GetDeviceAddress(context.device);

            Vk::SetDebugName(context.device, vertexBuffer.handle, fmt::format("ImGuiPass/VertexBuffer/{}", FIF));
        }

        if (indexBuffer.requestedSize < indexSize)
        {
            Vk::SetDebugName(context.device, indexBuffer.handle, fmt::format("ImGuiPass/Deleted/IndexBuffer/{}", FIF));

            m_deletionQueues[FIF].PushDeletor([allocator = context.allocator, indexBuffer] () mutable
            {
                indexBuffer.Destroy(allocator);
            });

            indexBuffer = Vk::Buffer
            (
                context.allocator,
                indexSize,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            Vk::SetDebugName(context.device, indexBuffer.handle, fmt::format("ImGuiPass/IndexBuffer/{}", FIF));
        }

        auto vertexDestination = static_cast<ImDrawVert*>(vertexBuffer.allocationInfo.pMappedData);
        auto indexDestination  = static_cast<ImDrawIdx*>(indexBuffer.allocationInfo.pMappedData);

        for (const auto drawList : drawData->CmdLists)
        {
            memcpy(vertexDestination, drawList->VtxBuffer.Data, drawList->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(indexDestination,  drawList->IdxBuffer.Data, drawList->IdxBuffer.Size * sizeof(ImDrawIdx));

            vertexDestination += drawList->VtxBuffer.Size;
            indexDestination  += drawList->IdxBuffer.Size;
        }

        vertexBuffer.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_HOST_BIT,
            VK_ACCESS_2_HOST_WRITE_BIT,
            VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
            0,
            vertexSize
        );

        indexBuffer.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_HOST_BIT,
            VK_ACCESS_2_HOST_WRITE_BIT,
            VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
            VK_ACCESS_2_INDEX_READ_BIT,
            0,
            indexSize
        );

        if (!(vertexBuffer.memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            Vk::CheckResult(vmaFlushAllocation(
                context.allocator,
                vertexBuffer.allocation,
                0,
                vertexSize),
                "Failed to flush allocation!"
            );
        }

        if (!(indexBuffer.memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            Vk::CheckResult(vmaFlushAllocation(
                context.allocator,
                indexBuffer.allocation,
                0,
                indexSize),
                "Failed to flush allocation!"
            );
        }
    }

    void RenderPass::Destroy(VkDevice device, VmaAllocator allocator)
    {
        Logger::Debug("{}\n", "Destroying ImGui pass!");

        pipeline.Destroy(device);

        for (auto& buffer : m_vertexBuffers)
        {
            buffer.Destroy(allocator);
        }

        for (auto& buffer : m_indexBuffers)
        {
            buffer.Destroy(allocator);
        }
    }
}
