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

#include "IndirectBuffer.h"

#include "MeshBuffer.h"
#include "Util/Log.h"
#include "Vulkan/DebugUtils.h"

namespace Renderer::Buffers
{
    IndirectBuffer::IndirectBuffer(VkDevice device, VmaAllocator allocator)
    {
        for (usize i = 0; i < drawCallBuffers.size(); ++i)
        {
            drawCallBuffers[i] = DrawCallBuffer(device, allocator, DrawCallBuffer::Type::CPUToGPU);

            Vk::SetDebugName(device, drawCallBuffers[i].drawCallBuffer.handle,  fmt::format("IndirectBuffer/DrawCallBuffer/DrawCalls/{}",   i));
            Vk::SetDebugName(device, drawCallBuffers[i].meshIndexBuffer.handle, fmt::format("IndirectBuffer/DrawCallBuffer/MeshIndices/{}", i));
        }

        frustumCulledDrawCallBuffer = DrawCallBuffer(device, allocator, DrawCallBuffer::Type::GPUOnly);

        occlusionCulledPreviouslyVisibleDrawCallBuffer = DrawCallBuffer(device, allocator, DrawCallBuffer::Type::GPUOnly);
        occlusionCulledNewlyVisibleDrawCallBuffer      = DrawCallBuffer(device, allocator, DrawCallBuffer::Type::GPUOnly);
        occlusionCulledTotalVisibleDrawCallBuffer      = DrawCallBuffer(device, allocator, DrawCallBuffer::Type::GPUOnly);

        visibilityBuffer = Vk::Buffer
        (
            allocator,
            sizeof(u32) * MAX_MESH_COUNT,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        );

        Vk::SetDebugName(device, frustumCulledDrawCallBuffer.drawCallBuffer.handle,  "IndirectBuffer/DrawCallBuffer/FrustumCulled/DrawCalls");
        Vk::SetDebugName(device, frustumCulledDrawCallBuffer.meshIndexBuffer.handle, "IndirectBuffer/DrawCallBuffer/FrustumCulled/MeshIndices");

        Vk::SetDebugName(device, occlusionCulledPreviouslyVisibleDrawCallBuffer.drawCallBuffer.handle,  "IndirectBuffer/DrawCallBuffer/OcclusionCulled/PreviouslyVisible/DrawCalls");
        Vk::SetDebugName(device, occlusionCulledPreviouslyVisibleDrawCallBuffer.meshIndexBuffer.handle, "IndirectBuffer/DrawCallBuffer/OcclusionCulled/PreviouslyVisible/MeshIndices");

        Vk::SetDebugName(device, occlusionCulledNewlyVisibleDrawCallBuffer.drawCallBuffer.handle,  "IndirectBuffer/DrawCallBuffer/OcclusionCulled/NewlyVisible/DrawCalls");
        Vk::SetDebugName(device, occlusionCulledNewlyVisibleDrawCallBuffer.meshIndexBuffer.handle, "IndirectBuffer/DrawCallBuffer/OcclusionCulled/NewlyVisible/MeshIndices");

        Vk::SetDebugName(device, occlusionCulledTotalVisibleDrawCallBuffer.drawCallBuffer.handle,  "IndirectBuffer/DrawCallBuffer/OcclusionCulled/TotalVisible/DrawCalls");
        Vk::SetDebugName(device, occlusionCulledTotalVisibleDrawCallBuffer.meshIndexBuffer.handle, "IndirectBuffer/DrawCallBuffer/OcclusionCulled/TotalVisible/MeshIndices");

        Vk::SetDebugName(device, visibilityBuffer.handle, "IndirectBuffer/MeshVisibilityLUT");
    }

    void IndirectBuffer::WriteDrawCalls
    (
        usize FIF,
        VmaAllocator allocator,
        const Models::ModelManager& modelManager,
        const std::span<const Renderer::RenderObject> renderObjects
    )
    {
        drawCallBuffers[FIF].WriteDrawCalls(allocator, modelManager, renderObjects);
    }

    void IndirectBuffer::Destroy(VmaAllocator allocator)
    {
        for (auto& buffer : drawCallBuffers)
        {
            buffer.Destroy(allocator);
        }

        frustumCulledDrawCallBuffer.Destroy(allocator);

        occlusionCulledPreviouslyVisibleDrawCallBuffer.Destroy(allocator);
        occlusionCulledNewlyVisibleDrawCallBuffer.Destroy(allocator);
        occlusionCulledTotalVisibleDrawCallBuffer.Destroy(allocator);

        visibilityBuffer.Destroy(allocator);
    }
}