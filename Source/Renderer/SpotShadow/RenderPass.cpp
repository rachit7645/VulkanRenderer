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

#include <Util/Maths.h>

#include "Renderer/RenderConstants.h"
#include "Vulkan/DebugUtils.h"
#include "Util/Log.h"

namespace Renderer::SpotShadow
{
    constexpr glm::uvec2 SHADOW_DIMENSIONS = {1024, 1024};
    constexpr glm::vec2  SHADOW_PLANES     = {0.1f, 100.0f};

    RenderPass::RenderPass
    (
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        Vk::FramebufferManager& framebufferManager
    )
        : pipeline(context, formatHelper),
          spotShadowBuffer(context.device, context.allocator)
    {
        for (usize i = 0; i < cmdBuffers.size(); ++i)
        {
            cmdBuffers[i] = Vk::CommandBuffer
            (
                context.device,
                context.commandPool,
                VK_COMMAND_BUFFER_LEVEL_PRIMARY
            );

            Vk::SetDebugName(context.device, cmdBuffers[i].handle, fmt::format("SpotShadowPass/FIF{}", i));
        }

        framebufferManager.AddFramebuffer
        (
            "SpotShadowMap",
            Vk::FramebufferType::Depth,
            Vk::ImageType::Single2D,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            Vk::FramebufferSize{
                .width       = SHADOW_DIMENSIONS.x,
                .height      = SHADOW_DIMENSIONS.y,
                .mipLevels   = 1,
                .arrayLayers = Objects::MAX_SPOT_LIGHT_COUNT
            }
        );

        framebufferManager.AddFramebufferView
        (
            "SpotShadowMap",
            "SpotShadowMapView",
            Vk::ImageType::Array2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = Objects::MAX_SPOT_LIGHT_COUNT,
            }
        );

        for (usize i = 0; i < Objects::MAX_SPOT_LIGHT_COUNT; ++i)
        {
            framebufferManager.AddFramebufferView
            (
                "SpotShadowMap",
                fmt::format("SpotShadowMapView/{}", i),
                Vk::ImageType::Single2D,
                Vk::FramebufferViewSize{
                    .baseMipLevel   = 0,
                    .levelCount     = 1,
                    .baseArrayLayer = static_cast<u32>(i),
                    .layerCount     = 1,
                }
            );
        }

        Logger::Info("{}\n", "Created spot shadow pass!");
    }

    void RenderPass::Destroy(VkDevice device, VmaAllocator allocator, VkCommandPool cmdPool)
    {
        Logger::Debug("{}\n", "Destroying spot shadow pass!");

        Vk::CommandBuffer::Free(device, cmdPool, cmdBuffers);

        spotShadowBuffer.Destroy(allocator);
        pipeline.Destroy(device);
    }

    void RenderPass::Render
    (
        usize FIF,
        VmaAllocator allocator,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::GeometryBuffer& geometryBuffer,
        const Buffers::MeshBuffer& meshBuffer,
        const Buffers::IndirectBuffer& indirectBuffer,
        Culling::Dispatch& cullingDispatch,
        const std::span<Objects::SpotLight>& lights
    )
    {
        const auto& currentCmdBuffer = cmdBuffers[FIF];

        currentCmdBuffer.Reset(0);
        currentCmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        Vk::BeginLabel(currentCmdBuffer, fmt::format("SpotShadowPass/FIF{}", FIF), glm::vec4(0.2196f, 0.2418f, 0.6588f, 1.0f));

        std::vector<glm::mat4> matrices = {};
        matrices.reserve(lights.size());

        const glm::mat4 projection = glm::perspective
        (
            glm::radians(90.0f),
            1.0f,
            SHADOW_PLANES.x,
            SHADOW_PLANES.y
        );

        for (const auto& light : lights)
        {
            const glm::mat4 view = glm::lookAt
            (
                light.position,
                glm::vec3(0.0f, 0.0f, 0.0f),
                glm::vec3(0.0f, 1.0f, 0.0f)
            );

            matrices.emplace_back(projection * view);
        }

        spotShadowBuffer.LoadMatrices(FIF, allocator, matrices);

        const auto& depthAttachment = framebufferManager.GetFramebuffer("SpotShadowMap");

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
            cullingDispatch.ComputeDispatch
            (
                FIF,
                matrices[i],
                currentCmdBuffer,
                meshBuffer,
                indirectBuffer
            );

            Vk::BeginLabel(currentCmdBuffer, fmt::format("Light #{}", i), glm::vec4(0.5146f, 0.7488f, 0.9388f, 1.0f));

            const auto depthAttachmentView = framebufferManager.GetFramebufferView(fmt::format("SpotShadowMapView/{}", i));

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
                .meshes        = meshBuffer.meshBuffers[FIF].deviceAddress,
                .visibleMeshes = meshBuffer.visibleMeshBuffer.deviceAddress,
                .positions     = geometryBuffer.positionBuffer.deviceAddress,
                .spotShadows   = spotShadowBuffer.buffers[FIF].deviceAddress,
                .currentIndex  = static_cast<u32>(i)
            };

            pipeline.LoadPushConstants
            (
               currentCmdBuffer,
               VK_SHADER_STAGE_VERTEX_BIT,
               0, sizeof(SpotShadow::PushConstant),
               reinterpret_cast<void*>(&pipeline.pushConstant)
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
}