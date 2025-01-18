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

#include "ForwardSceneBuffer.h"
#include "Util/Log.h"
#include "Renderer/RenderConstants.h"
#include "Util/Maths.h"
#include "Vulkan/Util.h"
#include "Externals/ImGui.h"

namespace Renderer::Forward
{
    ForwardPass::ForwardPass
    (
        Vk::Context& context,
        const Vk::TextureManager& textureManager,
        VkExtent2D extent
    )
        : pipeline(context, textureManager, GetColorFormat(context.physicalDevice), Vk::DepthBuffer::GetDepthFormat(context.physicalDevice))
    {
        for (usize i = 0; i < cmdBuffers.size(); ++i)
        {
            cmdBuffers[i] = Vk::CommandBuffer
            (
                context,
                VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                "ForwardPass/FIF" + std::to_string(i)
            );
        }

        InitData(context, extent);

        Logger::Info("{}\n", "Created forward pass!");
    }

    void ForwardPass::Recreate(const Vk::Context& context, VkExtent2D extent)
    {
        m_deletionQueue.FlushQueue();
        InitData(context, extent);
        Logger::Info("{}\n", "Recreated forward pass!");
    }

    void ForwardPass::Render
    (
        usize FIF,
        Vk::DescriptorCache& descriptorCache,
        const Models::ModelManager& modelManager,
        const Renderer::Camera& camera,
        const std::vector<Renderer::RenderObject>& renderObjects
    )
    {
        auto& currentCmdBuffer = cmdBuffers[FIF];

        currentCmdBuffer.Reset(0);
        currentCmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        image.Barrier
        (
            currentCmdBuffer,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            {
                .aspectMask     = image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        VkRenderingAttachmentInfo colorAttachmentInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = imageView.handle,
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

        VkRenderingAttachmentInfo depthAttachmentInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .pNext              = nullptr,
            .imageView          = depthBuffer.depthImageView.handle,
            .imageLayout        = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .resolveMode        = VK_RESOLVE_MODE_NONE,
            .resolveImageView   = VK_NULL_HANDLE,
            .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .loadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp            = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .clearValue         = {.depthStencil = {0.0f, 0x0}}
        };

        VkRenderingInfo renderInfo =
        {
            .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .pNext                = nullptr,
            .flags                = 0,
            .renderArea           = {
                .offset = {0, 0},
                .extent = {m_renderSize.x, m_renderSize.y}
            },
            .layerCount           = 1,
            .viewMask             = 0,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &colorAttachmentInfo,
            .pDepthAttachment     = &depthAttachmentInfo,
            .pStencilAttachment   = nullptr
        };

        vkCmdBeginRendering(currentCmdBuffer.handle, &renderInfo);

        pipeline.Bind(currentCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);

        VkViewport viewport =
        {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<f32>(m_renderSize.x),
            .height   = static_cast<f32>(m_renderSize.y),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vkCmdSetViewportWithCount(currentCmdBuffer.handle, 1, &viewport);

        VkRect2D scissor =
        {
            .offset = {0, 0},
            .extent = {m_renderSize.x, m_renderSize.y}
        };

        vkCmdSetScissorWithCount(currentCmdBuffer.handle, 1, &scissor);

        SceneBuffer sceneBuffer =
        {
            .projection = Maths::CreateProjectionReverseZ(
                camera.FOV,
                static_cast<f32>(m_renderSize.x) /
                static_cast<f32>(m_renderSize.y),
                PLANES.x,
                PLANES.y
            ),
            .view = camera.GetViewMatrix(),
            .cameraPos = {camera.position, 1.0f},
            .dirLight = {
                .position  = {-30.0f, -30.0f, -10.0f, 1.0f},
                .color     = {1.0f,   0.956f, 0.898f, 1.0f},
                .intensity = {5.0f,   5.0f,   5.0f,   1.0f}
            }
        };

        std::memcpy(pipeline.sceneSSBOs[FIF].allocInfo.pMappedData, &sceneBuffer, sizeof(sceneBuffer));

        pipeline.meshBuffer.LoadMeshes(FIF, modelManager, renderObjects);

        pipeline.pushConstant =
        {
            .scene    = pipeline.sceneSSBOs[FIF].deviceAddress,
            .meshes   = pipeline.meshBuffer.buffers[FIF].deviceAddress,
            .vertices = modelManager.geometryBuffer.vertexBuffer.deviceAddress,
            .drawID   = 0xFF
        };

        pipeline.LoadPushConstants
        (
            currentCmdBuffer,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, offsetof(Forward::PushConstant, drawID),
            reinterpret_cast<void*>(&pipeline.pushConstant)
        );

        // Scene specific descriptor sets
        std::array sceneDescriptorSets =
        {
            pipeline.GetStaticSet(descriptorCache).handle,
            modelManager.textureManager.textureSet.handle
        };

        pipeline.BindDescriptors
        (
            currentCmdBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            0,
            sceneDescriptorSets
        );

        modelManager.geometryBuffer.Bind(currentCmdBuffer);

        for (const auto& renderObject : renderObjects)
        {
            const auto& meshes = modelManager.GetModel(renderObject.modelID).meshes;

            for (usize i = 0; i < meshes.size(); ++i)
            {
                pipeline.pushConstant.drawID = i;
                pipeline.LoadPushConstants
                (
                    currentCmdBuffer,
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    offsetof(Forward::PushConstant, drawID), sizeof(Forward::PushConstant) - offsetof(Forward::PushConstant, drawID),
                    reinterpret_cast<void*>(&pipeline.pushConstant.drawID)
                );

                vkCmdDrawIndexed
                (
                    currentCmdBuffer.handle,
                    meshes[i].indexInfo.count,
                    1,
                    meshes[i].indexInfo.offset,
                    static_cast<s32>(meshes[i].vertexInfo.offset),
                    0
                );
            }
        }

        vkCmdEndRendering(currentCmdBuffer.handle);

        image.Barrier
        (
            currentCmdBuffer,
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            {
                .aspectMask     = image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );

        currentCmdBuffer.EndRecording();
    }

    void ForwardPass::InitData(const Vk::Context& context, VkExtent2D extent)
    {
        m_renderSize = {extent.width, extent.height};

        auto colorFormat = GetColorFormat(context.physicalDevice);
        Logger::Debug("Found forward color attachment format! [Format={}] \n", string_VkFormat(colorFormat));

        depthBuffer = Vk::DepthBuffer(context, extent);

        image = Vk::Image
        (
            context.allocator,
            m_renderSize.x,
            m_renderSize.y,
            1,
            colorFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        imageView = Vk::ImageView
        (
            context.device,
            image,
            VK_IMAGE_VIEW_TYPE_2D,
            image.format,
            VK_IMAGE_ASPECT_COLOR_BIT,
            0,
            1,
            0,
            1
        );

        Vk::ImmediateSubmit(context, [&] (const Vk::CommandBuffer& cmdBuffer)
        {
            image.Barrier
            (
                cmdBuffer,
                VK_PIPELINE_STAGE_2_NONE,
                VK_ACCESS_2_NONE,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                {
                    .aspectMask     = image.aspect,
                    .baseMipLevel   = 0,
                    .levelCount     = image.mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount     = 1
                }
            );
        });

        m_deletionQueue.PushDeletor([&] ()
        {
            imageView.Destroy(context.device);
            image.Destroy(context.allocator);
            depthBuffer.Destroy(context.device, context.allocator);
        });
    }

    VkFormat ForwardPass::GetColorFormat(VkPhysicalDevice physicalDevice) const
    {
        return Vk::FindSupportedFormat
        (
            physicalDevice,
            std::array
            {
                VK_FORMAT_B10G11R11_UFLOAT_PACK32,
                VK_FORMAT_R16G16B16A16_SFLOAT,
                VK_FORMAT_R32G32B32A32_SFLOAT
            },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_2_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT
        );
    }

    void ForwardPass::Destroy(const Vk::Context& context)
    {
        Logger::Debug("{}\n", "Destroying forward pass!");

        m_deletionQueue.FlushQueue();

        for (auto&& cmdBuffer : cmdBuffers)
        {
            cmdBuffer.Free(context);
        }

        pipeline.Destroy(context.device);
    }
}