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

#include "Dispatch.h"

#include "Vulkan/DebugUtils.h"
#include "Util/Log.h"
#include "Util/Plane.h"

namespace Renderer::Culling
{
    constexpr auto CULLING_WORKGROUP_SIZE = 64;

    Dispatch::Dispatch
    (
        const Vk::Context& context,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager
    )
        : frustumPipeline(context),
          visibilityPipeline(context),
          occlusionPipeline(context, megaSet, textureManager),
          frustumBuffer(context.device, context.allocator)
    {
        Logger::Info("{}\n", "Created culling dispatch pass!");
    }

    void Dispatch::DispatchFrustumCulling
    (
        usize FIF,
        const glm::mat4& projectionView,
        const Vk::CommandBuffer& cmdBuffer,
        const Buffers::MeshBuffer& meshBuffer,
        const Buffers::IndirectBuffer& indirectBuffer
    )
    {
        Vk::BeginLabel(cmdBuffer, fmt::format("FrustumCullingDispatch/FIF{}", FIF), glm::vec4(0.6196f, 0.5588f, 0.8588f, 1.0f));

        frustumBuffer.LoadPlanes(cmdBuffer, projectionView);

        frustumPipeline.Bind(cmdBuffer);

        frustumPipeline.pushConstant =
        {
            .meshes            = meshBuffer.buffers[FIF].deviceAddress,
            .drawCalls         = indirectBuffer.drawCallBuffers[FIF].drawCallBuffer.deviceAddress,
            .culledDrawCalls   = indirectBuffer.frustumCulledDrawCallBuffer.drawCallBuffer.deviceAddress,
            .culledMeshIndices = indirectBuffer.frustumCulledDrawCallBuffer.meshIndexBuffer.deviceAddress,
            .frustum           = frustumBuffer.buffer.deviceAddress
        };

        frustumPipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0,
            sizeof(Frustum::PushConstant),
            &frustumPipeline.pushConstant
        );

        indirectBuffer.frustumCulledDrawCallBuffer.drawCallBuffer.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
            VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            0,
            sizeof(u32) + sizeof(VkDrawIndexedIndirectCommand) * indirectBuffer.drawCallBuffers[FIF].writtenDrawCount
        );

        indirectBuffer.frustumCulledDrawCallBuffer.meshIndexBuffer.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            0,
            sizeof(u32) * indirectBuffer.drawCallBuffers[FIF].writtenDrawCount
        );

        vkCmdDispatch
        (
            cmdBuffer.handle,
            (indirectBuffer.drawCallBuffers[FIF].writtenDrawCount + CULLING_WORKGROUP_SIZE - 1) / CULLING_WORKGROUP_SIZE,
            1,
            1
        );

        indirectBuffer.frustumCulledDrawCallBuffer.drawCallBuffer.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
            VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
            0,
            sizeof(u32) + sizeof(VkDrawIndexedIndirectCommand) * indirectBuffer.drawCallBuffers[FIF].writtenDrawCount
        );

        indirectBuffer.frustumCulledDrawCallBuffer.meshIndexBuffer.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
            0,
            sizeof(u32) * indirectBuffer.drawCallBuffers[FIF].writtenDrawCount
        );

        Vk::EndLabel(cmdBuffer);
    }

    void Dispatch::DispatchOcclusionCulling
    (
        usize FIF,
        const glm::mat4& projectionView,
        const Vk::CommandBuffer& cmdBuffer,
        const Vk::FramebufferManager& framebufferManager,
        const Vk::MegaSet& megaSet,
        const Buffers::SceneBuffer& sceneBuffer,
        const Buffers::MeshBuffer& meshBuffer,
        const Buffers::IndirectBuffer& indirectBuffer
    )
    {
        const usize previousFIF = (FIF + Vk::FRAMES_IN_FLIGHT - 1) % Vk::FRAMES_IN_FLIGHT;

        const u32 currentDrawCount  = indirectBuffer.drawCallBuffers[FIF].writtenDrawCount;
        const u32 previousDrawCount = indirectBuffer.drawCallBuffers[previousFIF].writtenDrawCount;

        const VkDeviceSize currentDrawSize      = sizeof(u32) + sizeof(VkDrawIndexedIndirectCommand) * currentDrawCount;
        const VkDeviceSize previousDrawSize     = sizeof(u32) + sizeof(VkDrawIndexedIndirectCommand) * previousDrawCount;
        const VkDeviceSize currentMeshIndexSize = sizeof(u32) * currentDrawCount;

        const u32 groupCount = (currentDrawCount + CULLING_WORKGROUP_SIZE - 1) / CULLING_WORKGROUP_SIZE;

        Vk::BeginLabel(cmdBuffer, fmt::format("OcclusionCullingDispatch/FIF{}", FIF), glm::vec4(0.6196f, 0.5588f, 0.8588f, 1.0f));

        Vk::BeginLabel(cmdBuffer, "Visibility", glm::vec4(0.3196f, 0.5588f, 0.8588f, 1.0f));

        visibilityPipeline.Bind(cmdBuffer);

        visibilityPipeline.pushConstant =
        {
            .drawCalls         = indirectBuffer.drawCallBuffers[FIF].drawCallBuffer.deviceAddress,
            .previousDrawCalls = indirectBuffer.occlusionCulledPreviouslyVisibleDrawCallBuffer.drawCallBuffer.deviceAddress,
            .previouslyVisible = indirectBuffer.visibilityBuffer.deviceAddress
        };

        visibilityPipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(Visibility::PushConstant),
            &visibilityPipeline.pushConstant
        );

        indirectBuffer.occlusionCulledPreviouslyVisibleDrawCallBuffer.drawCallBuffer.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
            VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
            0,
            previousDrawSize
        );

        indirectBuffer.visibilityBuffer.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            0,
            currentMeshIndexSize
        );

        vkCmdDispatch
        (
            cmdBuffer.handle,
            groupCount,
            1,
            1
        );

        indirectBuffer.visibilityBuffer.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
            0,
            currentMeshIndexSize
        );

        Vk::EndLabel(cmdBuffer);

        Vk::BeginLabel(cmdBuffer, "Occlusion", glm::vec4(0.6196f, 0.5588f, 0.8588f, 1.0f));

        frustumBuffer.LoadPlanes(cmdBuffer, projectionView);

        occlusionPipeline.Bind(cmdBuffer);

        occlusionPipeline.pushConstant =
        {
            .scene                   = sceneBuffer.buffers[FIF].deviceAddress,
            .meshes                  = meshBuffer.buffers[FIF].deviceAddress,
            .frustum                 = frustumBuffer.buffer.deviceAddress,
            .drawCalls               = indirectBuffer.drawCallBuffers[FIF].drawCallBuffer.deviceAddress,
            .newlyVisibleDrawCalls   = indirectBuffer.occlusionCulledNewlyVisibleDrawCallBuffer.drawCallBuffer.deviceAddress,
            .newlyVisibleMeshIndices = indirectBuffer.occlusionCulledNewlyVisibleDrawCallBuffer.meshIndexBuffer.deviceAddress,
            .totalVisibleDrawCalls   = indirectBuffer.occlusionCulledTotalVisibleDrawCallBuffer.drawCallBuffer.deviceAddress,
            .totalVisibleMeshIndices = indirectBuffer.occlusionCulledTotalVisibleDrawCallBuffer.meshIndexBuffer.deviceAddress,
            .previouslyVisible       = indirectBuffer.visibilityBuffer.deviceAddress,
            .hiZDepthIndex           = framebufferManager.GetFramebufferView(fmt::format("HiZView/{}", FIF)).sampledImageIndex,
            .samplerIndex            = occlusionPipeline.samplerIndex
        };

        occlusionPipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(Occlusion::PushConstant),
            &occlusionPipeline.pushConstant
        );

        const std::array descriptorSets = {megaSet.descriptorSet};
        occlusionPipeline.BindDescriptors(cmdBuffer, 0, descriptorSets);

        indirectBuffer.occlusionCulledNewlyVisibleDrawCallBuffer.drawCallBuffer.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
            VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            0,
            currentDrawSize
        );

        indirectBuffer.occlusionCulledNewlyVisibleDrawCallBuffer.meshIndexBuffer.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            0,
            currentMeshIndexSize
        );

        indirectBuffer.occlusionCulledTotalVisibleDrawCallBuffer.drawCallBuffer.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            0,
            currentDrawSize
        );

        indirectBuffer.occlusionCulledTotalVisibleDrawCallBuffer.meshIndexBuffer.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            0,
            currentMeshIndexSize
        );

        vkCmdDispatch
        (
            cmdBuffer.handle,
            groupCount,
            1,
            1
        );

        indirectBuffer.occlusionCulledNewlyVisibleDrawCallBuffer.drawCallBuffer.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
            VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
            0,
            currentDrawSize
        );

        indirectBuffer.occlusionCulledNewlyVisibleDrawCallBuffer.meshIndexBuffer.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
            0,
            currentMeshIndexSize
        );

        indirectBuffer.occlusionCulledTotalVisibleDrawCallBuffer.drawCallBuffer.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
            VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
            0,
            currentDrawSize
        );

        indirectBuffer.occlusionCulledTotalVisibleDrawCallBuffer.meshIndexBuffer.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
            0,
            currentMeshIndexSize
        );

        Vk::EndLabel(cmdBuffer);

        Vk::EndLabel(cmdBuffer);
    }

    void Dispatch::Destroy(VkDevice device, VmaAllocator allocator)
    {
        Logger::Debug("{}\n", "Destroying culling dispatch pass!");

        frustumBuffer.Destroy(allocator);

        frustumPipeline.Destroy(device);
        visibilityPipeline.Destroy(device);
        occlusionPipeline.Destroy(device);
    }
}
