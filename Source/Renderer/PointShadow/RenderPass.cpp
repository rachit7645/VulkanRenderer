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

namespace Renderer::PointShadow
{
    constexpr glm::uvec2 POINT_SHADOW_DIMENSIONS = {512,  512};
    constexpr glm::vec2  POINT_SHADOW_PLANES     = {1.0f, 25.0f};

    RenderPass::RenderPass
    (
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        Vk::FramebufferManager& framebufferManager
    )
        : pipeline(context, formatHelper),
          pointShadowBuffer(context.device, context.allocator)
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

        framebufferManager.AddFramebuffer
        (
            "PointShadowMap",
            Vk::FramebufferType::Depth,
            Vk::FramebufferImageType::ArrayCube,
            Vk::FramebufferUsage::Sampled,
            Vk::FramebufferSize{
                .width       = POINT_SHADOW_DIMENSIONS.x,
                .height      = POINT_SHADOW_DIMENSIONS.y,
                .mipLevels   = 1,
                .arrayLayers = 6 * Objects::MAX_POINT_LIGHT_COUNT
            }
        );

        framebufferManager.AddFramebufferView
        (
            "PointShadowMap",
            "PointShadowMapView",
            Vk::FramebufferImageType::ArrayCube,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 6 * Objects::MAX_POINT_LIGHT_COUNT,
            }
        );

        for (usize i = 0; i < Objects::MAX_POINT_LIGHT_COUNT; ++i)
        {
            for (usize j = 0; j < 6; ++j)
            {
                framebufferManager.AddFramebufferView
                (
                    "PointShadowMap",
                    fmt::format("PointShadowMapView/Light{}/{}", i, j),
                    Vk::FramebufferImageType::Single2D,
                    Vk::FramebufferViewSize{
                        .baseMipLevel   = 0,
                        .levelCount     = 1,
                        .baseArrayLayer = static_cast<u32>(6 * i + j),
                        .layerCount     = 1,
                    }
                );
            }
        }

        Logger::Info("{}\n", "Created point shadow pass!");
    }

    void RenderPass::Render
    (
        usize FIF,
        VmaAllocator allocator,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::GeometryBuffer& geometryBuffer,
        const Buffers::SceneBuffer& sceneBuffer,
        const Buffers::MeshBuffer& meshBuffer,
        const Buffers::IndirectBuffer& indirectBuffer,
        Culling::Dispatch& cullingDispatch,
        const std::span<Objects::PointLight> lights
    )
    {
        const auto& currentCmdBuffer = cmdBuffers[FIF];

        currentCmdBuffer.Reset(0);
        currentCmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        Vk::BeginLabel(currentCmdBuffer, fmt::format("PointShadowPass/FIF{}", FIF), glm::vec4(0.4196f, 0.6488f, 0.9588f, 1.0f));

        std::vector<PointShadow::PointShadowData> pointShadowData;
        pointShadowData.resize(lights.size());

        const auto projection = glm::perspectiveRH_ZO
        (
            glm::radians(90.0f),
            static_cast<f32>(POINT_SHADOW_DIMENSIONS.x) / static_cast<f32>(POINT_SHADOW_DIMENSIONS.y),
            POINT_SHADOW_PLANES.x,
            POINT_SHADOW_PLANES.y
        );

        for (usize i = 0; i < lights.size(); ++i)
        {
            pointShadowData[i] = PointShadow::PointShadowData{
                .matrices     = {
                    projection * glm::lookAt(lights[i].position, lights[i].position + glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
                    projection * glm::lookAt(lights[i].position, lights[i].position + glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
                    projection * glm::lookAt(lights[i].position, lights[i].position + glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
                    projection * glm::lookAt(lights[i].position, lights[i].position + glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
                    projection * glm::lookAt(lights[i].position, lights[i].position + glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
                    projection * glm::lookAt(lights[i].position, lights[i].position + glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
                },
                .shadowPlanes = POINT_SHADOW_PLANES
            };
        }

        pointShadowBuffer.LoadPointShadowData(FIF, allocator, pointShadowData);

        const auto& depthAttachment = framebufferManager.GetFramebuffer("PointShadowMap");

        depthAttachment.image.Barrier
        (
            currentCmdBuffer,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            {
                .aspectMask     = depthAttachment.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = depthAttachment.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = depthAttachment.image.arrayLayers
            }
        );

        for (usize i = 0; i < lights.size(); ++i)
        {
            Vk::BeginLabel(currentCmdBuffer, fmt::format("Light #{}", i), glm::vec4(0.7146f, 0.2488f, 0.9388f, 1.0f));

            for (usize face = 0; face < 6; ++face)
            {
                Vk::BeginLabel(currentCmdBuffer, fmt::format("Face #{}", face), glm::vec4(0.6146f, 0.8488f, 0.3388f, 1.0f));

                cullingDispatch.ComputeDispatch
                (
                    FIF,
                    pointShadowData[i].matrices[face],
                    currentCmdBuffer,
                    meshBuffer,
                    indirectBuffer
                );

                const auto depthAttachmentView = framebufferManager.GetFramebufferView(fmt::format("PointShadowMapView/Light{}/{}", i, face));

                const VkRenderingAttachmentInfo depthAttachmentInfo =
                {
                    .sType              = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .pNext              = nullptr,
                    .imageView          = depthAttachmentView.view.handle,
                    .imageLayout        = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                    .resolveMode        = VK_RESOLVE_MODE_NONE,
                    .resolveImageView   = VK_NULL_HANDLE,
                    .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .loadOp             = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp            = VK_ATTACHMENT_STORE_OP_STORE,
                    .clearValue         = {.depthStencil = {1.0f, 0x0}}
                };

                const VkRenderingInfo renderInfo =
                {
                    .sType                = VK_STRUCTURE_TYPE_RENDERING_INFO,
                    .pNext                = nullptr,
                    .flags                = 0,
                    .renderArea           = {
                        .offset = {0, 0},
                        .extent = {depthAttachment.image.width, depthAttachment.image.height}
                    },
                    .layerCount           = 1,
                    .viewMask             = 0,
                    .colorAttachmentCount = 0,
                    .pColorAttachments    = nullptr,
                    .pDepthAttachment     = &depthAttachmentInfo,
                    .pStencilAttachment   = nullptr
                };

                vkCmdBeginRendering(currentCmdBuffer.handle, &renderInfo);

                pipeline.Bind(currentCmdBuffer);

                const VkViewport viewport =
                {
                    .x        = 0.0f,
                    .y        = 0.0f,
                    .width    = static_cast<f32>(depthAttachment.image.width),
                    .height   = static_cast<f32>(depthAttachment.image.height),
                    .minDepth = 0.0f,
                    .maxDepth = 1.0f
                };

                vkCmdSetViewportWithCount(currentCmdBuffer.handle, 1, &viewport);

                const VkRect2D scissor =
                {
                    .offset = {0, 0},
                    .extent = {depthAttachment.image.width, depthAttachment.image.height}
                };

                vkCmdSetScissorWithCount(currentCmdBuffer.handle, 1, &scissor);

                pipeline.pushConstant =
                {
                    .scene         = sceneBuffer.buffers[FIF].deviceAddress,
                    .meshes        = meshBuffer.meshBuffers[FIF].deviceAddress,
                    .visibleMeshes = meshBuffer.visibilityBuffer.deviceAddress,
                    .positions     = geometryBuffer.positionBuffer.deviceAddress,
                    .pointShadows  = pointShadowBuffer.buffers[FIF].deviceAddress,
                    .lightIndex    = static_cast<u32>(i),
                    .faceIndex     = static_cast<u32>(face)
                };

                pipeline.PushConstants
                (
                   currentCmdBuffer,
                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                   0, sizeof(PointShadow::PushConstant),
                   &pipeline.pushConstant
                );

                geometryBuffer.Bind(currentCmdBuffer);

                vkCmdDrawIndexedIndirectCount
                (
                    currentCmdBuffer.handle,
                    indirectBuffer.culledDrawCallBuffer.handle,
                    sizeof(u32),
                    indirectBuffer.culledDrawCallBuffer.handle,
                    0,
                    indirectBuffer.writtenDrawCount,
                    sizeof(VkDrawIndexedIndirectCommand)
                );

                vkCmdEndRendering(currentCmdBuffer.handle);

                Vk::EndLabel(currentCmdBuffer);
            }

            Vk::EndLabel(currentCmdBuffer);
        }

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

    void RenderPass::Destroy(VkDevice device, VmaAllocator allocator, VkCommandPool cmdPool)
    {
        Logger::Debug("{}\n", "Destroying point shadow pass!");

        pointShadowBuffer.Destroy(allocator);

        Vk::CommandBuffer::Free(device, cmdPool, cmdBuffers);

        pipeline.Destroy(device);
    }
}
