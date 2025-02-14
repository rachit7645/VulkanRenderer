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

namespace Renderer::Shadow
{
    constexpr glm::uvec2 SHADOW_DIMENSIONS = {2048, 2048};

    RenderPass::RenderPass
    (
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        Vk::FramebufferManager& framebufferManager
    )
        : pipeline(context, formatHelper),
          cascadeBuffer(context.device, context.allocator)
    {
        for (usize i = 0; i < cmdBuffers.size(); ++i)
        {
            cmdBuffers[i] = Vk::CommandBuffer
            (
                context.device,
                context.commandPool,
                VK_COMMAND_BUFFER_LEVEL_PRIMARY
            );

            Vk::SetDebugName(context.device, cmdBuffers[i].handle, fmt::format("ShadowPass/FIF{}", i));
        }

        framebufferManager.AddFramebuffer
        (
            "ShadowCascades",
            Vk::FramebufferType::Depth,
            Vk::ImageType::Single2D,
            Vk::FramebufferSize{
                .width       = SHADOW_DIMENSIONS.x,
                .height      = SHADOW_DIMENSIONS.y,
                .mipLevels   = 1,
                .arrayLayers = Shadow::CASCADE_COUNT
            }
        );

        framebufferManager.AddFramebufferView
        (
            "ShadowCascades",
            "ShadowCascadesView",
            Vk::ImageType::Array2D,
            {
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = Shadow::CASCADE_COUNT,
            }
        );

        Logger::Info("{}\n", "Created shadow pass!");
    }

    void RenderPass::Destroy(VkDevice device, VmaAllocator allocator, VkCommandPool cmdPool)
    {
        Logger::Debug("{}\n", "Destroying shadow pass!");

        Vk::CommandBuffer::Free(device, cmdPool, cmdBuffers);

        cascadeBuffer.Destroy(allocator);
        pipeline.Destroy(device);
    }

    void RenderPass::Render
    (
        usize FIF,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::GeometryBuffer& geometryBuffer,
        const Buffers::MeshBuffer& meshBuffer,
        const Buffers::IndirectBuffer& indirectBuffer,
        const Objects::Camera& camera,
        const Objects::DirLight& light
    )
    {
        const auto& currentCmdBuffer = cmdBuffers[FIF];

        currentCmdBuffer.Reset(0);
        currentCmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        Vk::BeginLabel(currentCmdBuffer, fmt::format("ShadowPass/FIF{}", FIF), glm::vec4(0.7196f, 0.2488f, 0.6588f, 1.0f));

        const auto& depthAttachmentView = framebufferManager.GetFramebufferView("ShadowCascadesView");
        const auto& depthAttachment     = framebufferManager.GetFramebuffer(depthAttachmentView.framebuffer);

        const auto& sceneColor = framebufferManager.GetFramebuffer("SceneColor");
        const auto aspectRatio = static_cast<f32>(sceneColor.image.width) / static_cast<f32>(sceneColor.image.height);

        cascadeBuffer.LoadCascades(FIF, CalculateCascades(aspectRatio, camera, light));

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
            .viewMask             = Shadow::CASCADES_VIEW_MASK,
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
            .meshes    = meshBuffer.buffers[FIF].deviceAddress,
            .positions = geometryBuffer.positionBuffer.deviceAddress,
            .cascades  = cascadeBuffer.buffers[FIF].deviceAddress,
            .offset    = 0 * Shadow::CASCADE_COUNT
        };

        pipeline.LoadPushConstants
        (
           currentCmdBuffer,
           VK_SHADER_STAGE_VERTEX_BIT,
           0, sizeof(Shadow::PushConstant),
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

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Cascades"))
            {
                ImGui::DragFloat("Cascade Split Lambda", &m_cascadeSplitLambda, 0.005f, 0.0f, 1.0f, "%.3f");
                ImGui::DragFloat("Cascade Offset",       &m_cascadeOffset,      0.005f, 1.0f, 5.0f, "%.3f");

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        Vk::EndLabel(currentCmdBuffer);

        currentCmdBuffer.EndRecording();
    }

    // https://github.com/SaschaWillems/Vulkan/blob/master/examples/shadowmappingcascade/shadowmappingcascade.cpp
    std::array<Shadow::Cascade, CASCADE_COUNT> RenderPass::CalculateCascades(f32 aspectRatio, const Objects::Camera& camera, const Objects::DirLight& light)
	{
        std::array<Shadow::Cascade, CASCADE_COUNT> cascades      = {};
		std::array<f32,             CASCADE_COUNT> cascadeSplits = {};

		const f32 nearClip  = Renderer::PLANES.x;
		const f32 farClip   = Renderer::PLANES.y;
		const f32 clipRange = farClip - nearClip;

		const f32 minZ = nearClip;
		const f32 maxZ = nearClip + clipRange;

		const f32 range = maxZ - minZ;
		const f32 ratio = maxZ / minZ;
        
		// https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch10.html
		for (u32 i = 0; i < CASCADE_COUNT; i++)
		{
			const f32 p       = static_cast<f32>(i + 1) / static_cast<f32>(CASCADE_COUNT);
			const f32 log     = minZ * std::pow(ratio, p);
			const f32 uniform = minZ + range * p;
			const f32 d       = m_cascadeSplitLambda * (log - uniform) + uniform;

			cascadeSplits[i] = (d - nearClip) / clipRange;
		}

		// Calculate orthographic projection matrix for each cascade
		f32 lastSplitDist = 0.0f;
		for (u32 i = 0; i < CASCADE_COUNT; i++)
		{
			const auto splitDist = cascadeSplits[i];

			std::array frustumCorners =
			{
				glm::vec3(-1.0f,  1.0f, 0.0f),
				glm::vec3( 1.0f,  1.0f, 0.0f),
				glm::vec3( 1.0f, -1.0f, 0.0f),
				glm::vec3(-1.0f, -1.0f, 0.0f),
				glm::vec3(-1.0f,  1.0f, 1.0f),
				glm::vec3( 1.0f,  1.0f, 1.0f),
				glm::vec3( 1.0f, -1.0f, 1.0f),
				glm::vec3(-1.0f, -1.0f, 1.0f),
			};

			// Project frustum corners into world space
			const glm::mat4 invCam = glm::inverse(glm::perspective(camera.FOV, aspectRatio, nearClip, farClip) * camera.GetViewMatrix());
			for (u32 j = 0; j < 8; j++)
			{
				const glm::vec4 invCorner = invCam * glm::vec4(frustumCorners[j], 1.0f);
			    frustumCorners[j]         = invCorner / invCorner.w;
			}

			for (u32 j = 0; j < 4; j++)
			{
				const glm::vec3 dist  = frustumCorners[j + 4] - frustumCorners[j];
				frustumCorners[j + 4] = frustumCorners[j] + (dist * splitDist);
				frustumCorners[j]     = frustumCorners[j] + (dist * lastSplitDist);
			}

			// Get frustum center
			auto frustumCenter = glm::vec3(0.0f);
			for (u32 j = 0; j < 8; j++)
			{
				frustumCenter += frustumCorners[j];
			}
			frustumCenter /= 8.0f;

			f32 radius = 0.0f;
			for (u32 j = 0; j < 8; j++)
			{
				const f32 distance = glm::length(frustumCorners[j] - frustumCenter);
				radius             = glm::max(radius, distance);
			}
			radius = std::ceil(radius * 16.0f) / 16.0f;
		    radius *= m_cascadeOffset;

		    const f32 texelSize = (radius * 2.0f) / SHADOW_DIMENSIONS.x;

		    frustumCenter.x = std::floor(frustumCenter.x / texelSize) * texelSize;
		    frustumCenter.y = std::floor(frustumCenter.y / texelSize) * texelSize;
		    frustumCenter.z = std::floor(frustumCenter.z / texelSize) * texelSize;

			const glm::vec3 maxExtents = glm::vec3(radius);
			const glm::vec3 minExtents = -maxExtents;

			const glm::vec3 lightDir         = glm::normalize(-light.position);
			const glm::mat4 lightViewMatrix  = glm::lookAt(frustumCenter - lightDir * -minExtents.z, frustumCenter, glm::vec3(0.0f, 1.0f, 0.0f));
			const glm::mat4 lightOrthoMatrix = glm::ortho(minExtents.x, maxExtents.x, minExtents.y, maxExtents.y, 0.0f, maxExtents.z - minExtents.z);

			// Store split distance and matrix in cascade
			cascades[i].distance = (nearClip + splitDist * clipRange) * -1.0f;
			cascades[i].matrix   = lightOrthoMatrix * lightViewMatrix;

			lastSplitDist = cascadeSplits[i];
		}

        return cascades;
	}
}