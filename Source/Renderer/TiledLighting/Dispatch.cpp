/*
 * Copyright (c) 2023 - 2026 Rachit
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

#include "Dispatch.h"

#include "TiledLighting/Common.h"
#include "TiledLighting/Bounds.h"
#include "TiledLighting/Culling.h"
#include "Vulkan/DebugUtils.h"

namespace Renderer::TiledLighting
{
    Dispatch::Dispatch
    (
        const Vk::MegaSet& megaSet,
        Vk::PipelineManager& pipelineManager,
        Vk::FramebufferManager& framebufferManager
    )
    {
        pipelineManager.AddPipeline("TiledLighting/Bounds", Vk::PipelineConfig{}
            .SetPipelineType(VK_PIPELINE_BIND_POINT_COMPUTE)
            .AttachShader("TiledLighting/Bounds.comp", VK_SHADER_STAGE_COMPUTE_BIT)
            .AddPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Bounds::Constants))
            .AddDescriptorLayout(megaSet.descriptorLayout)
        );

        pipelineManager.AddPipeline("TiledLighting/Culling", Vk::PipelineConfig{}
            .SetPipelineType(VK_PIPELINE_BIND_POINT_COMPUTE)
            .AttachShader("TiledLighting/Culling.comp", VK_SHADER_STAGE_COMPUTE_BIT)
            .AddPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Culling::Constants))
            .AddDescriptorLayout(megaSet.descriptorLayout)
        );

        framebufferManager.AddFramebuffer
        (
            "TiledLighting/TileDepths",
            VK_FORMAT_R32G32_SFLOAT,
            VK_IMAGE_VIEW_TYPE_2D,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            [] (const VkExtent2D& renderExtent, ENGINE_UNUSED const VkExtent2D& displayExtent) -> Vk::FramebufferSize
            {
                return
                {
                    .width       = (renderExtent.width  + TILE_SIZE - 1) / TILE_SIZE,
                    .height      = (renderExtent.height + TILE_SIZE - 1) / TILE_SIZE,
                    .mipLevels   = 1,
                    .arrayLayers = 1
                };
            },
            Vk::FramebufferInitialState{
                .stageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .accessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .layout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            }
        );

        framebufferManager.AddFramebufferView
        (
            "TiledLighting/TileDepths",
            "TiledLighting/TileDepthsView",
            VK_IMAGE_VIEW_TYPE_2D,
            Vk::FramebufferViewSize{
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1
            }
        );
    }

    void Dispatch::Execute
    (
        usize FIF,
        VkDevice device,
        VmaAllocator allocator,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Vk::TextureManager& textureManager,
        const Buffers::SceneBuffer& sceneBuffer,
        const Objects::GlobalSamplers& samplers,
        Buffers::TileLightIndexBuffer& tileLightIndexBuffer,
        Util::DeletionQueue& deletionQueue
    )
    {
        Vk::BeginLabel(cmdBuffer, "Tiled Lighting", glm::vec4(0.4098f, 0.2843f, 0.7599f, 1.0f));

        Bounds
        (
            cmdBuffer,
            pipelineManager,
            framebufferManager,
            megaSet,
            textureManager,
            samplers
        );

        Culling
        (
            FIF,
            device,
            allocator,
            cmdBuffer,
            pipelineManager,
            framebufferManager,
            megaSet,
            textureManager,
            sceneBuffer,
            samplers,
            tileLightIndexBuffer,
            deletionQueue
        );

        Vk::EndLabel(cmdBuffer);
    }

    void Dispatch::Bounds
    (
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Vk::TextureManager& textureManager,
        const Objects::GlobalSamplers& samplers
    )
    {
        Vk::BeginLabel(cmdBuffer, "Bounds", glm::vec4(0.6098f, 0.8423f, 0.3599f, 1.0f));

        const auto& pipeline = pipelineManager.GetPipeline("TiledLighting/Bounds");

        const auto& tileDepths = framebufferManager.GetFramebuffer("TiledLighting/TileDepths");

        tileDepths.image.Barrier
        (
            cmdBuffer,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .newLayout      = VK_IMAGE_LAYOUT_GENERAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = tileDepths.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = tileDepths.image.arrayLayers
            }
        );

        pipeline.Bind(cmdBuffer);

        const auto constants = Bounds::Constants
        {
            .PointSamplerIndex  = textureManager.GetSampler(samplers.pointSamplerID).descriptorID,
            .SceneDepthIndex    = framebufferManager.GetFramebufferView("SceneDepthView").sampledImageID,
            .OutTileDepthsIndex = framebufferManager.GetFramebufferView("TiledLighting/TileDepthsView").storageImageID
        };

        pipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_COMPUTE_BIT,
            constants
        );

        pipeline.BindDescriptors(cmdBuffer, megaSet);

        vkCmdDispatch
        (
            cmdBuffer.handle,
            tileDepths.image.width,
            tileDepths.image.height,
            1
        );

        tileDepths.image.Barrier
        (
            cmdBuffer,
            Vk::ImageBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .oldLayout      = VK_IMAGE_LAYOUT_GENERAL,
                .newLayout      = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .baseMipLevel   = 0,
                .levelCount     = tileDepths.image.mipLevels,
                .baseArrayLayer = 0,
                .layerCount     = tileDepths.image.arrayLayers
            }
        );

        Vk::EndLabel(cmdBuffer);
    }

    void Dispatch::Culling
    (
        usize FIF,
        VkDevice device,
        VmaAllocator allocator,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::PipelineManager& pipelineManager,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Vk::TextureManager& textureManager,
        const Buffers::SceneBuffer& sceneBuffer,
        const Objects::GlobalSamplers& samplers,
        Buffers::TileLightIndexBuffer& tileLightIndexBuffer,
        Util::DeletionQueue& deletionQueue
    )
    {
        Vk::BeginLabel(cmdBuffer, "Culling", glm::vec4(0.6984f, 0.3423f, 0.3599f, 1.0f));

        const auto& tileDepths = framebufferManager.GetFramebuffer("TiledLighting/TileDepths");

        tileLightIndexBuffer.Update
        (
            device,
            allocator,
            cmdBuffer,
            tileDepths.image.width * tileDepths.image.height,
            deletionQueue
        );

        tileLightIndexBuffer.resizableBuffer.buffer.Barrier
        (
            cmdBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = tileLightIndexBuffer.resizableBuffer.buffer.size
            }
        );

        const auto& pipeline = pipelineManager.GetPipeline("TiledLighting/Culling");

        pipeline.Bind(cmdBuffer);

        const auto constants = Culling::Constants
        {
            .Scene             = sceneBuffer.graphicsBuffers.sceneBuffers[FIF].deviceAddress,
            .TileLightIndices  = tileLightIndexBuffer.resizableBuffer.buffer.deviceAddress,
            .PointSamplerIndex = textureManager.GetSampler(samplers.pointSamplerID).descriptorID,
            .TileDepthsIndex   = framebufferManager.GetFramebufferView("TiledLighting/TileDepthsView").sampledImageID,
            .MaxTileID         = glm::uvec2(tileDepths.image.width - 1, tileDepths.image.height - 1)
        };

        pipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_COMPUTE_BIT,
            constants
        );

        pipeline.BindDescriptors(cmdBuffer, megaSet);

        vkCmdDispatch
        (
            cmdBuffer.handle,
            (tileDepths.image.width  + 8 - 1) / 8,
            (tileDepths.image.height + 8 - 1) / 8,
            1
        );

        tileLightIndexBuffer.resizableBuffer.buffer.Barrier
        (
            cmdBuffer,
            Vk::BufferBarrier{
                .srcStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask  = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .dstStageMask   = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask  = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .srcQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamily = VK_QUEUE_FAMILY_IGNORED,
                .offset         = 0,
                .size           = tileLightIndexBuffer.resizableBuffer.buffer.size
            }
        );

        Vk::EndLabel(cmdBuffer);
    }
}
