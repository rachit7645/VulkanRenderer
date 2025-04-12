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
    constexpr auto WORKGROUP_SIZE = 64;

    Dispatch::Dispatch(const Vk::Context& context)
        : pipeline(context),
          frustumBuffer(context.device, context.allocator)
    {
        Logger::Info("{}\n", "Created culling dispatch pass!");
    }

    void Dispatch::ComputeDispatch
    (
        usize FIF,
        const glm::mat4& projectionView,
        const Vk::CommandBuffer& cmdBuffer,
        const Buffers::MeshBuffer& meshBuffer,
        const Buffers::IndirectBuffer& indirectBuffer
    )
    {
        Vk::BeginLabel(cmdBuffer, fmt::format("CullingDispatch/FIF{}", FIF), glm::vec4(0.6196f, 0.5588f, 0.8588f, 1.0f));

        frustumBuffer.LoadPlanes(FIF, cmdBuffer, projectionView);

        pipeline.Bind(cmdBuffer);

        pipeline.pushConstant =
        {
            .meshes            = meshBuffer.meshBuffers[FIF].deviceAddress,
            .drawCalls         = indirectBuffer.drawCallBuffers[FIF].drawCallBuffer.deviceAddress,
            .culledDrawCalls   = indirectBuffer.culledDrawCallBuffer.drawCallBuffer.deviceAddress,
            .culledMeshIndices = indirectBuffer.culledDrawCallBuffer.meshIndexBuffer.deviceAddress,
            .frustum           = frustumBuffer.buffers[FIF].deviceAddress
        };

        pipeline.PushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0,
            sizeof(Culling::PushConstant),
            &pipeline.pushConstant
        );

        indirectBuffer.culledDrawCallBuffer.drawCallBuffer.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
            VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            0,
            sizeof(u32) + sizeof(VkDrawIndexedIndirectCommand) * indirectBuffer.drawCallBuffers[FIF].writtenDrawCount
        );

        vkCmdDispatch
        (
            cmdBuffer.handle,
            (indirectBuffer.drawCallBuffers[FIF].writtenDrawCount / WORKGROUP_SIZE) + 1,
            1,
            1
        );

        indirectBuffer.culledDrawCallBuffer.drawCallBuffer.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,
            VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT,
            0,
            sizeof(u32) + sizeof(VkDrawIndexedIndirectCommand) * indirectBuffer.drawCallBuffers[FIF].writtenDrawCount
        );

        indirectBuffer.culledDrawCallBuffer.meshIndexBuffer.Barrier
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

    void Dispatch::Destroy(VkDevice device, VmaAllocator allocator)
    {
        Logger::Debug("{}\n", "Destroying culling dispatch pass!");

        frustumBuffer.Destroy(allocator);

        pipeline.Destroy(device);
    }
}
