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

namespace Renderer::Culling
{
    Dispatch::Dispatch(const Vk::Context& context)
        : pipeline(context)
    {
        Logger::Info("{}\n", "Created culling dispatch pass!");
    }

    void Dispatch::ComputeDispatch
    (
        usize FIF,
        const Vk::CommandBuffer& cmdBuffer,
        const Buffers::MeshBuffer& meshBuffer,
        const Buffers::IndirectBuffer& indirectBuffer
    )
    {
        Vk::BeginLabel(cmdBuffer, fmt::format("CullingDispatch/FIF{}", FIF), glm::vec4(0.6196f, 0.5588f, 0.8588f, 1.0f));

        pipeline.Bind(cmdBuffer);

        pipeline.pushConstant =
        {
            .meshes          = meshBuffer.meshBuffers[FIF].deviceAddress,
            .visibleMeshes   = meshBuffer.visibleMeshBuffer.deviceAddress,
            .frustum         = indirectBuffer.drawCallBuffers[FIF].deviceAddress,
            .drawCalls       = indirectBuffer.drawCallBuffers[FIF].deviceAddress,
            .culledDrawCalls = indirectBuffer.culledDrawCallBuffer.deviceAddress
        };

        pipeline.LoadPushConstants
        (
            cmdBuffer,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0,
            sizeof(Culling::PushConstant),
            &pipeline.pushConstant
        );

        vkCmdDispatch
        (
            cmdBuffer.handle,
            static_cast<u32>(std::ceil(static_cast<f32>(indirectBuffer.writtenDrawCount) / 32.0f)),
            1,
            1
        );

        meshBuffer.visibleMeshBuffer.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
            0,
            sizeof(u32) * indirectBuffer.writtenDrawCount
        );

        indirectBuffer.culledDrawCallBuffer.Barrier
        (
            cmdBuffer,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
            VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
            VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
            0,
            sizeof(u32) + sizeof(VkDrawIndexedIndirectCommand) * indirectBuffer.writtenDrawCount
        );

        Vk::EndLabel(cmdBuffer);
    }

    void Dispatch::Destroy(VkDevice device)
    {
        Logger::Debug("{}\n", "Destroying culling dispatch pass!");

        pipeline.Destroy(device);
    }
}
