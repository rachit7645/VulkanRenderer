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

#include "RayDispatch.h"

#include "Vulkan/DebugUtils.h"
#include "Util/Log.h"
#include "Renderer/Depth/RenderPass.h"
#include "Shadows/ShadowRT.h"

namespace Renderer::ShadowRT
{
    constexpr u32 MISS_SHADER_GROUP_COUNT = 1;
    constexpr u32 HIT_SHADER_GROUP_COUNT  = 1;

    RayDispatch::RayDispatch
    (
        const Vk::Context& context,
        Vk::CommandBufferAllocator& cmdBufferAllocator,
        Vk::FramebufferManager& framebufferManager,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager
    )
        : m_pipeline(context, megaSet, textureManager),
          m_shaderBindingTable(context, cmdBufferAllocator, m_pipeline, MISS_SHADER_GROUP_COUNT, HIT_SHADER_GROUP_COUNT)
    {
        framebufferManager.AddFramebuffer
        (
            "ShadowRT",
            Vk::FramebufferType::ColorR_Unorm8,
            Vk::FramebufferImageType::Single2D,
            Vk::FramebufferUsage::Sampled | Vk::FramebufferUsage::Storage | Vk::FramebufferUsage::TransferDestination,
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

    void RayDispatch::TraceRays
    (
        usize FIF,
        usize frameIndex,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::MegaSet& megaSet,
        const Models::ModelManager& modelManager,
        const Vk::FramebufferManager& framebufferManager,
        const Buffers::SceneBuffer& sceneBuffer,
        const Buffers::MeshBuffer& meshBuffer,
        const Vk::AccelerationStructure& accelerationStructure
    )
    {
        Vk::BeginLabel(cmdBuffer, "Raytraced Shadows", glm::vec4(0.4196f, 0.2488f, 0.6588f, 1.0f));

        const auto& shadowMapView = framebufferManager.GetFramebufferView("ShadowRTView");
        const auto& shadowMap     = framebufferManager.GetFramebuffer(shadowMapView.framebuffer);

        if (accelerationStructure.topLevelASes[FIF].handle == VK_NULL_HANDLE)
        {
            shadowMap.image.Barrier
            (
                cmdBuffer,
                Vk::ImageBarrier{
                    .srcStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                    .dstStageMask   = VK_PIPELINE_STAGE_2_CLEAR_BIT,
                    .dstAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .newLayout      = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .baseMipLevel   = 0,
                    .levelCount     = shadowMap.image.mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount     = shadowMap.image.arrayLayers
                }
            );

            constexpr VkClearColorValue WHITE = {.float32 = {1.0f, 1.0f, 1.0f, 1.0f}};

            const VkImageSubresourceRange subresourceRange =
            {
                .aspectMask     = shadowMap.image.aspect,
                .baseMipLevel   = 0,
                .levelCount     = shadowMap.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = shadowMap.image.arrayLayers
            };

            vkCmdClearColorImage
            (
                cmdBuffer.handle,
                shadowMap.image.handle,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                &WHITE,
                1,
                &subresourceRange
            );

            shadowMap.image.Barrier
            (
                cmdBuffer,
                Vk::ImageBarrier{
                    .srcStageMask   = VK_PIPELINE_STAGE_2_CLEAR_BIT,
                    .srcAccessMask  = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                    .dstStageMask   = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                    .oldLayout      = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    .baseMipLevel   = 0,
                    .levelCount     = shadowMap.image.mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount     = shadowMap.image.arrayLayers
                }
            );

            return;
        }

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

        m_pipeline.Bind(cmdBuffer);

        const auto constants = ShadowRT::Constants
        {
            .TLAS                = accelerationStructure.topLevelASes[FIF].deviceAddress,
            .Scene               = sceneBuffer.buffers[FIF].deviceAddress,
            .Meshes              = meshBuffer.GetCurrentBuffer(frameIndex).deviceAddress,
            .Indices             = modelManager.geometryBuffer.GetIndexBuffer().deviceAddress,
            .Vertices            = modelManager.geometryBuffer.GetVertexBuffer().deviceAddress,
            .GBufferSamplerIndex = modelManager.textureManager.GetSampler(m_pipeline.gBufferSamplerID).descriptorID,
            .TextureSamplerIndex = modelManager.textureManager.GetSampler(m_pipeline.textureSamplerID).descriptorID,
            .GNormalIndex        = framebufferManager.GetFramebufferView("GNormalView").sampledImageID,
            .SceneDepthIndex     = framebufferManager.GetFramebufferView("SceneDepthView").sampledImageID,
            .OutputImage         = shadowMapView.storageImageID
        };

        m_pipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
            constants
        );

        const std::array descriptorSets = {megaSet.descriptorSet};
        m_pipeline.BindDescriptors(cmdBuffer, 0, descriptorSets);

        // You still need an empty callable region for some reason
        constexpr VkStridedDeviceAddressRegionKHR emptyCallableRegion = {};

        vkCmdTraceRaysKHR
        (
            cmdBuffer.handle,
            &m_shaderBindingTable.raygenRegion,
            &m_shaderBindingTable.missRegion,
            &m_shaderBindingTable.hitRegion,
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

    void RayDispatch::Destroy(VkDevice device, VmaAllocator allocator)
    {
        m_shaderBindingTable.Destroy(allocator);
        m_pipeline.Destroy(device);
    }
}