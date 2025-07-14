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
        const Vk::Swapchain& swapchain,
        const Vk::MegaSet& megaSet,
        Vk::PipelineManager& pipelineManager
    )
    {
        constexpr std::array DYNAMIC_STATES = {VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT, VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT};

        const std::array colorFormats = {swapchain.imageFormat};
        
        pipelineManager.AddPipeline("DearImGui", Vk::PipelineConfig{}
            .SetPipelineType(VK_PIPELINE_BIND_POINT_GRAPHICS)
            .SetRenderingInfo(0, colorFormats, VK_FORMAT_UNDEFINED)
            .AttachShader("ImGui/ImGui.vert", VK_SHADER_STAGE_VERTEX_BIT)
            .AttachShader("ImGui/ImGui.frag", VK_SHADER_STAGE_FRAGMENT_BIT)
            .SetDynamicStates(DYNAMIC_STATES)
            .SetIAState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetRasterizerState(VK_FALSE, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL)
            .AddBlendAttachment(
                VK_TRUE,
                VK_BLEND_FACTOR_SRC_ALPHA,
                VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                VK_BLEND_OP_ADD,
                VK_BLEND_FACTOR_ONE,
                VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                VK_BLEND_OP_ADD,
                VK_COLOR_COMPONENT_R_BIT |
                VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT |
                VK_COLOR_COMPONENT_A_BIT
            )
            .AddPushConstant(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DearImGui::Constants))
            .AddDescriptorLayout(megaSet.descriptorLayout)
        );
    }

    void RenderPass::Render
    (
        usize FIF,
        VkDevice device,
        VmaAllocator allocator,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::Swapchain& swapchain,
        const Objects::GlobalSamplers& samplers,
        Vk::MegaSet& megaSet,
        Models::ModelManager& modelManager,
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
                pipelineManager,
                swapchain,
                samplers,
                megaSet,
                modelManager,
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
        const Vk::PipelineManager& pipelineManager,
        const Vk::Swapchain& swapchain,
        const Objects::GlobalSamplers& samplers,
        Vk::MegaSet& megaSet,
        Models::ModelManager& modelManager,
        Util::DeletionQueue& deletionQueue,
        const ImDrawData* drawData
    )
    {
        const auto& pipeline = pipelineManager.GetPipeline("DearImGui");
        
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

        UpdateTextures
        (
            device,
            allocator,
            cmdBuffer,
            megaSet,
            modelManager,
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

        pipeline.Bind(cmdBuffer);

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
        constants.SamplerIndex = modelManager.textureManager.GetSampler(samplers.imguiSamplerID).descriptorID;

        pipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            offsetof(DearImGui::Constants, TextureIndex),
            &constants
        );

        const std::array descriptorSets = {megaSet.descriptorSet};
        pipeline.BindDescriptors(cmdBuffer, 0, descriptorSets);

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

                pipeline.PushConstants
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
                0,
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
                0,
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            Vk::SetDebugName(device, indexBuffer.handle, fmt::format("ImGuiPass/IndexBuffer/{}", FIF));
        }

        auto vertexDestination = static_cast<ImDrawVert*>(vertexBuffer.hostAddress);
        auto indexDestination  = static_cast<ImDrawIdx*>(indexBuffer.hostAddress);

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

    void RenderPass::UpdateTextures
    (
        VkDevice device,
        VmaAllocator allocator,
        const Vk::CommandBuffer& cmdBuffer,
        Vk::MegaSet& megaSet,
        Models::ModelManager& modelManager,
        Util::DeletionQueue& deletionQueue,
        const ImDrawData* drawData
    )
    {
        if (drawData->Textures == nullptr)
        {
            return;
        }

        for (auto* texture : *drawData->Textures)
        {
            if (texture->Status == ImTextureStatus_OK)
            {
                continue;
            }

            if (texture->Status == ImTextureStatus_WantCreate)
            {
                if (texture->TexID != ImTextureID_Invalid || texture->BackendUserData != nullptr)
                {
                    Logger::Error("Texture already created! [ID={}]", texture->UniqueID);
                }

                if (texture->Format != ImTextureFormat_RGBA32)
                {
                    Logger::Error("Unsupported texture format! [ID={}]", texture->UniqueID);
                }

                const auto pixels = static_cast<u8*>(texture->GetPixels());

                const auto id = modelManager.textureManager.AddTexture
                (
                    allocator,
                    deletionQueue,
                    Vk::ImageUpload{
                        .type   = Vk::ImageUploadType::RAW,
                        .flags  = Vk::ImageUploadFlags::None,
                        .source = Vk::ImageUploadRawMemory{
                            .name   = fmt::format("DearImGui/Texture/{}", texture->UniqueID),
                            .width  = static_cast<u32>(texture->Width),
                            .height = static_cast<u32>(texture->Height),
                            .format = VK_FORMAT_R8G8B8A8_UNORM,
                            .data   = std::vector(pixels, pixels + texture->GetSizeInBytes()),
                        }
                    }
                );

                texture->BackendUserData = std::bit_cast<void*>(id);
            }

            if (texture->Status == ImTextureStatus_WantUpdates)
            {
                const auto id = std::bit_cast<Vk::TextureID>(texture->BackendUserData);

                const VkDeviceSize rowPitch = texture->UpdateRect.w * texture->BytesPerPixel;

                auto data = std::vector<u8>(texture->UpdateRect.h * rowPitch);

                for (u32 row = 0; row < texture->UpdateRect.h; ++row)
                {
                    const void* srcRow = texture->GetPixelsAt(texture->UpdateRect.x, texture->UpdateRect.y + row);
                          void* dstRow = data.data() + (row * rowPitch);

                    std::memcpy(dstRow, srcRow, rowPitch);
                }

                modelManager.textureManager.UpdateTexture
                (
                    id,
                    allocator,
                    deletionQueue,
                    Vk::ImageUpdateRawMemory{
                        .update = {
                            .offset = {texture->UpdateRect.x, texture->UpdateRect.y},
                            .extent = {texture->UpdateRect.w, texture->UpdateRect.h}
                        },
                        .data = data
                    }
                );

                texture->SetStatus(ImTextureStatus_OK);
            }

            if (texture->Status == ImTextureStatus_WantDestroy)
            {
                const auto id = std::bit_cast<Vk::TextureID>(texture->BackendUserData);

                modelManager.textureManager.DestroyTexture
                (
                    id,
                    device,
                    allocator,
                    megaSet,
                    deletionQueue
                );

                texture->SetTexID(ImTextureID_Invalid);
                texture->SetStatus(ImTextureStatus_Destroyed);
                texture->BackendUserData = nullptr;
            }
        }

        modelManager.Update
        (
            cmdBuffer,
            device,
            allocator,
            megaSet,
            deletionQueue
        );

        megaSet.Update(device);

        for (auto* texture : *drawData->Textures)
        {
            if (texture->Status != ImTextureStatus_WantCreate)
            {
                continue;
            }

            const auto id = std::bit_cast<Vk::TextureID>(texture->BackendUserData);

            texture->SetTexID(modelManager.textureManager.GetTexture(id).descriptorID);
            texture->SetStatus(ImTextureStatus_OK);
        }
    }

    void RenderPass::Destroy(VmaAllocator allocator)
    {
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
