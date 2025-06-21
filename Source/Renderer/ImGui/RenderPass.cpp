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
#include "Vulkan/BarrierWriter.h"
#include "Util/Log.h"
#include "ImGui/DearImGui.h"

namespace Renderer::DearImGui
{
    RenderPass::RenderPass
    (
        const Vk::Context& context,
        const Vk::Swapchain& swapchain,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager
    )
        : m_pipeline(context, megaSet, textureManager, swapchain.imageFormat)
    {
    }

    void RenderPass::Render
    (
        usize FIF,
        VkDevice device,
        VmaAllocator allocator,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::MegaSet& megaSet,
        const Vk::TextureManager& textureManager,
        const Vk::Swapchain& swapchain,
        Util::DeletionQueue& deletionQueue
    )
    {
        ImGui::Render();

        const auto drawData = ImGui::GetDrawData();

        Vk::BeginLabel(cmdBuffer, "Dear ImGui", glm::vec4(0.9137f, 0.4745f, 0.9882f, 1.0f));

        if (drawData->TotalVtxCount > 0)
        {
            RenderGui
            (
                FIF,
                device,
                allocator,
                cmdBuffer,
                megaSet,
                textureManager,
                swapchain,
                deletionQueue,
                drawData
            );
        }

        auto& swapchainImage = swapchain.images[swapchain.imageIndex];

        swapchainImage.Barrier
        (
            cmdBuffer,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask  = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                .dstAccessMask  = VK_ACCESS_2_NONE,
                .oldLayout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = swapchainImage.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = swapchainImage.arrayLayers
            }
        );

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::RenderGui
    (
        usize FIF,
        VkDevice device,
        VmaAllocator allocator,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::MegaSet& megaSet,
        const Vk::TextureManager& textureManager,
        const Vk::Swapchain& swapchain,
        Util::DeletionQueue& deletionQueue,
        const ImDrawData* drawData
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
            device,
            allocator,
            cmdBuffer,
            currentVertexBuffer,
            currentIndexBuffer,
            deletionQueue,
            drawData
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

        m_pipeline.Bind(cmdBuffer);

        vkCmdBindIndexBuffer
        (
            cmdBuffer.handle,
            currentIndexBuffer.handle,
            0,
            sizeof(ImDrawIdx) == sizeof(u16) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32
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

        DearImGui::Constants constants = {};

        constants.Vertices     = currentVertexBuffer.deviceAddress;
        constants.Scale        = glm::vec2(2.0f) / displaySize;
        constants.Translate    = glm::vec2(-1.0f) - (displayPos * constants.Scale);
        constants.SamplerIndex = textureManager.GetSampler(m_pipeline.samplerID).descriptorID;

        m_pipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            offsetof(DearImGui::Constants, TextureIndex),
            &constants
        );

        const std::array descriptorSets = {megaSet.descriptorSet};
        m_pipeline.BindDescriptors(cmdBuffer, 0, descriptorSets);

        s32 globalVertexOffset = 0;
        s32 globalIndexOffset  = 0;

        for (const auto drawList : drawData->CmdLists)
        {
            for (const auto& cmd : drawList->CmdBuffer)
            {
                auto clipMin = (glm::vec2(cmd.ClipRect.x, cmd.ClipRect.y) - displayPos) * framebufferScale;
                auto clipMax = (glm::vec2(cmd.ClipRect.z, cmd.ClipRect.w) - displayPos) * framebufferScale;

                clipMin = glm::clamp(clipMin, glm::zero<glm::vec2>(), resolution);

                if (glm::any(glm::lessThanEqual(clipMax, clipMin)))
                {
                    continue;
                }

                const VkRect2D scissor =
                {
                    .offset = {static_cast<s32>(clipMin.x), static_cast<s32>(clipMin.y)},
                    .extent = {static_cast<u32>(clipMax.x - clipMin.x), static_cast<u32>(clipMax.y - clipMin.y)}
                };

                vkCmdSetScissorWithCount(cmdBuffer.handle, 1, &scissor);

                constants.TextureIndex = cmd.GetTexID();

                m_pipeline.PushConstants
                (
                    cmdBuffer,
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    offsetof(Constants, TextureIndex),
                    sizeof(u32),
                    &constants.TextureIndex
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
        VkDevice device,
        VmaAllocator allocator,
        const Vk::CommandBuffer& cmdBuffer,
        Vk::Buffer& vertexBuffer,
        Vk::Buffer& indexBuffer,
        Util::DeletionQueue& deletionQueue,
        const ImDrawData* drawData
    )
    {
        // Create or resize the vertex/index buffers
        const VkDeviceSize vertexSize = drawData->TotalVtxCount * sizeof(ImDrawVert);
        const VkDeviceSize indexSize  = drawData->TotalIdxCount * sizeof(ImDrawIdx);

        if (vertexBuffer.size < vertexSize)
        {
            Vk::SetDebugName(device, vertexBuffer.handle, fmt::format("ImGuiPass/Deleted/VertexBuffer/{}", FIF));

            deletionQueue.PushDeletor([allocator, vertexBuffer] () mutable
            {
                vertexBuffer.Destroy(allocator);
            });

            vertexBuffer = Vk::Buffer
            (
                allocator,
                vertexSize,
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            vertexBuffer.GetDeviceAddress(device);

            Vk::SetDebugName(device, vertexBuffer.handle, fmt::format("ImGuiPass/VertexBuffer/{}", FIF));
        }

        if (indexBuffer.size < indexSize)
        {
            Vk::SetDebugName(device, indexBuffer.handle, fmt::format("ImGuiPass/Deleted/IndexBuffer/{}", FIF));

            deletionQueue.PushDeletor([allocator, indexBuffer] () mutable
            {
                indexBuffer.Destroy(allocator);
            });

            indexBuffer = Vk::Buffer
            (
                allocator,
                indexSize,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            Vk::SetDebugName(device, indexBuffer.handle, fmt::format("ImGuiPass/IndexBuffer/{}", FIF));
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

        Vk::BarrierWriter{}
        .WriteBufferBarrier(
            vertexBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_HOST_BIT,
                .srcAccessMask  = VK_ACCESS_2_HOST_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = vertexSize
            }
        )
        .WriteBufferBarrier(
            indexBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_HOST_BIT,
                .srcAccessMask  = VK_ACCESS_2_HOST_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT,
                .dstAccessMask  = VK_ACCESS_2_INDEX_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = indexSize
            }
        )
        .Execute(cmdBuffer);

        const bool needsManualFlushing = !(vertexBuffer.memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) ||
                                         !(indexBuffer.memoryProperties  & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (needsManualFlushing)
        {
            const std::array<VmaAllocation, 2> allocations = {vertexBuffer.allocation, indexBuffer.allocation};
            const std::array<VkDeviceSize,  2> offsets     = {0, 0};
            const std::array<VkDeviceSize,  2> sizes       = {vertexSize, indexSize};

            Vk::CheckResult(vmaFlushAllocations(
                allocator,
                allocations.size(),
                allocations.data(),
                offsets.data(),
                sizes.data()),
                "Failed to flush allocations!"
            );
        }
    }

    void RenderPass::Destroy(VkDevice device, VmaAllocator allocator)
    {
        m_pipeline.Destroy(device);

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
