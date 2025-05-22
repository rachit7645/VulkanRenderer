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
#include "Renderer/Depth/RenderPass.h"
#include "Shadows/ShadowRT.h"

namespace Renderer::ShadowRT
{
    constexpr u32 MISS_SHADER_GROUP_COUNT = 1;
    constexpr u32 HIT_SHADER_GROUP_COUNT  = 1;

    RenderPass::RenderPass
    (
        const Vk::Context& context,
        Vk::CommandBufferAllocator& cmdBufferAllocator,
        Vk::FramebufferManager& framebufferManager,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager
    )
        : pipeline(context, megaSet, textureManager),
          shaderBindingTable(context, cmdBufferAllocator, pipeline, MISS_SHADER_GROUP_COUNT, HIT_SHADER_GROUP_COUNT)
    {
        framebufferManager.AddFramebuffer
        (
            "ShadowRT",
            Vk::FramebufferType::ColorR_Unorm8,
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferUsage::Sampled | Vk::FramebufferUsage::Storage,
            [] (const VkExtent2D& extent) -> Vk::FramebufferSize
            {
                return
                {
                    .width       = extent.width,
                    .height      = extent.height,
                    .mipLevels   = 1,
                    .arrayLayers = 1
                };
            },
            {
                .dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
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
    }

    void RenderPass::Render
    (
        usize FIF,
        usize frameIndex,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::MegaSet& megaSet,
        const Vk::FramebufferManager& framebufferManager,
        const Buffers::SceneBuffer& sceneBuffer,
        const Buffers::MeshBuffer& meshBuffer,
        const Vk::GeometryBuffer& geometryBuffer,
        const Vk::AccelerationStructure& accelerationStructure
    )
    {
        Vk::BeginLabel(cmdBuffer, "ShadowRTPass", glm::vec4(0.4196f, 0.2488f, 0.6588f, 1.0f));

        const auto& shadowMapView = framebufferManager.GetFramebufferView("ShadowRTView");
        const auto& shadowMap     = framebufferManager.GetFramebuffer(shadowMapView.framebuffer);

        shadowMap.image.Barrier
        (
            cmdBuffer,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_GENERAL,
                .baseMipLevel   = 0,
                .levelCount     = shadowMap.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = shadowMap.image.arrayLayers
            }
        );

        pipeline.Bind(cmdBuffer);

        const auto constants = ShadowRT::Constants
        {
            .TLAS                = accelerationStructure.topLevelASes[FIF].deviceAddress,
            .Scene               = sceneBuffer.buffers[FIF].deviceAddress,
            .Meshes              = meshBuffer.GetCurrentBuffer(frameIndex).deviceAddress,
            .Indices             = geometryBuffer.indexBuffer.buffer.deviceAddress,
            .Vertices            = geometryBuffer.vertexBuffer.buffer.deviceAddress,
            .GBufferSamplerIndex = pipeline.gBufferSamplerIndex,
            .TextureSamplerIndex = pipeline.textureSamplerIndex,
            .GNormalIndex        = framebufferManager.GetFramebufferView("GNormal_Rgh_Mtl_View").sampledImageIndex,
            .SceneDepthIndex     = framebufferManager.GetFramebufferView("SceneDepthView").sampledImageIndex,
            .OutputImage         = shadowMapView.storageImageIndex
        };

        pipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
            constants
        );

        const std::array descriptorSets = {megaSet.descriptorSet};
        pipeline.BindDescriptors(cmdBuffer, 0, descriptorSets);

        // You still need an empty callable region for some reason
        constexpr VkStridedDeviceAddressRegionKHR emptyCallableRegion = {};

        vkCmdTraceRaysKHR
        (
            cmdBuffer.handle,
            &shaderBindingTable.raygenRegion,
            &shaderBindingTable.missRegion,
            &shaderBindingTable.hitRegion,
            &emptyCallableRegion,
            shadowMap.image.width,
            shadowMap.image.height,
            1
        );

        shadowMap.image.Barrier
        (
            cmdBuffer,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_GENERAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .baseMipLevel   = 0,
                .levelCount     = shadowMap.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = shadowMap.image.arrayLayers
            }
        );

        Vk::EndLabel(cmdBuffer);
    }

    void RenderPass::Destroy(VkDevice device, VmaAllocator allocator)
    {
        shaderBindingTable.Destroy(allocator);
        pipeline.Destroy(device);
    }
}