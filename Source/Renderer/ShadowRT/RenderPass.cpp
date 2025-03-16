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

namespace Renderer::ShadowRT
{
    RenderPass::RenderPass
    (
        const Vk::Context& context,
        Vk::FramebufferManager& framebufferManager,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager
    )
        : pipeline(context, megaSet, textureManager),
          shaderBindingTable(context, pipeline, 1, 0, 0)
    {
        for (usize i = 0; i < cmdBuffers.size(); ++i)
        {
            cmdBuffers[i] = Vk::CommandBuffer
            (
                context.device,
                context.commandPool,
                VK_COMMAND_BUFFER_LEVEL_PRIMARY
            );

            Vk::SetDebugName(context.device, cmdBuffers[i].handle, fmt::format("ShadowRTPass/FIF{}", i));
        }

        framebufferManager.AddFramebuffer
        (
            "ShadowRT",
            Vk::FramebufferType::ColorR_Norm8,
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferUsage::Sampled | Vk::FramebufferUsage::Storage,
            [] (const VkExtent2D& extent, UNUSED Vk::FramebufferManager& framebufferManager) -> Vk::FramebufferSize
            {
                return
                {
                    .width       = extent.width,
                    .height      = extent.height,
                    .mipLevels   = 1,
                    .arrayLayers = 1
                };
            }
        );

        framebufferManager.AddFramebufferView
        (
            "ShadowRT",
            "ShadowRTView",
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            }
        );

        Logger::Info("{}\n", "Created shadow pass!");
    }

    void RenderPass::Render
    (
        usize FIF,
        VkDevice device,
        VmaAllocator allocator,
        const Vk::MegaSet& megaSet,
        const Vk::FramebufferManager& framebufferManager,
        const Buffers::SceneBuffer& sceneBuffer,
        Vk::AccelerationStructure& accelerationStructure,
        const std::span<const Renderer::RenderObject> renderObjects
    )
    {
        const auto& currentCmdBuffer = cmdBuffers[FIF];

        currentCmdBuffer.Reset(0);
        currentCmdBuffer.BeginRecording(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

        accelerationStructure.BuildTopLevelAS
        (
            FIF,
            currentCmdBuffer,
            device,
            allocator,
            renderObjects
        );

        Vk::BeginLabel(currentCmdBuffer, fmt::format("ShadowRTPass/FIF{}", FIF), glm::vec4(0.4196f, 0.2488f, 0.6588f, 1.0f));

        const auto& shadowMapView = framebufferManager.GetFramebufferView("ShadowRTView");
        const auto& shadowMap     = framebufferManager.GetFramebuffer(shadowMapView.framebuffer);

        shadowMap.image.Barrier
        (
            currentCmdBuffer,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_GENERAL,
            {
                .aspectMask     = shadowMap.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = shadowMap.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = shadowMap.image.arrayLayers
            }
        );

        pipeline.Bind(currentCmdBuffer);

        pipeline.pushConstant =
        {
            .tlas                = accelerationStructure.topLevelASes[FIF].deviceAddress,
            .scene               = sceneBuffer.buffers[FIF].deviceAddress,
            .gBufferSamplerIndex = pipeline.gBufferSamplerIndex,
            .gNormalIndex        = framebufferManager.GetFramebufferView("GNormal_Rgh_Mtl_View").sampledImageIndex,
            .sceneDepthIndex     = framebufferManager.GetFramebufferView("SceneDepthView").sampledImageIndex,
            .outputImage         = shadowMapView.storageImageIndex
        };

        pipeline.PushConstants
        (
            currentCmdBuffer,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            0, sizeof(ShadowRT::PushConstant),
            &pipeline.pushConstant
        );

        const std::array descriptorSets = {megaSet.descriptorSet};
        pipeline.BindDescriptors(currentCmdBuffer, 0, descriptorSets);

        vkCmdTraceRaysKHR
        (
            currentCmdBuffer.handle,
            &shaderBindingTable.raygenRegion,
            &shaderBindingTable.missRegion,
            &shaderBindingTable.hitRegion,
            &shaderBindingTable.callableRegion,
            shadowMap.image.width,
            shadowMap.image.height,
            1
        );

        shadowMap.image.Barrier
        (
            currentCmdBuffer,
            VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            {
                .aspectMask     = shadowMap.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = shadowMap.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = shadowMap.image.arrayLayers
            }
        );

        Vk::EndLabel(currentCmdBuffer);

        currentCmdBuffer.EndRecording();
    }

    void RenderPass::Destroy(VkDevice device, VmaAllocator allocator, VkCommandPool cmdPool)
    {
        Logger::Debug("{}\n", "Destroying shadow pass!");

        shaderBindingTable.Destroy(allocator);

        Vk::CommandBuffer::Free(device, cmdPool, cmdBuffers);

        pipeline.Destroy(device);
    }
}