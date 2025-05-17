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
        for (usize i = 0; i < writtenDrawCallBuffers.size(); ++i)
        {
            writtenDrawCallBuffers[i] = DrawCallBuffer(device, allocator, DrawCallBuffer::Type::CPUToGPU);

            Vk::SetDebugName(device, writtenDrawCallBuffers[i].drawCallBuffer.handle, fmt::format("IndirectBuffer/DrawCallBuffer/DrawCalls/{}", i));
        }

        frustumCulledOpaqueBuffer            = DrawCallBuffer(device, allocator, DrawCallBuffer::Type::GPUOnly);
        frustumCulledOpaqueDoubleSidedBuffer = DrawCallBuffer(device, allocator, DrawCallBuffer::Type::GPUOnly);

        Vk::SetDebugName(device, frustumCulledOpaqueBuffer.drawCallBuffer.handle,              "IndirectBuffer/DrawCallBuffer/FrustumCulled/Opaque/DrawCalls");
        Vk::SetDebugName(device, frustumCulledOpaqueBuffer.meshIndexBuffer->handle,            "IndirectBuffer/DrawCallBuffer/FrustumCulled/Opaque/MeshIndices");
        Vk::SetDebugName(device, frustumCulledOpaqueDoubleSidedBuffer.drawCallBuffer.handle,   "IndirectBuffer/DrawCallBuffer/FrustumCulled/Opaque/DoubleSided/DrawCalls");
        Vk::SetDebugName(device, frustumCulledOpaqueDoubleSidedBuffer.meshIndexBuffer->handle, "IndirectBuffer/DrawCallBuffer/FrustumCulled/Opaque/DoubleSided/MeshIndices");
    }

    void IndirectBuffer::WriteDrawCalls
    (
        usize FIF,
        VmaAllocator allocator,
        const Models::ModelManager& modelManager,
        const std::span<const Renderer::RenderObject> renderObjects
    )
    {
        writtenDrawCallBuffers[FIF].WriteDrawCalls(allocator, modelManager, renderObjects);
    }

    void IndirectBuffer::Destroy(VmaAllocator allocator)
    {
        for (auto& buffer : writtenDrawCallBuffers)
        {
            buffer.Destroy(allocator);
        }

        frustumCulledOpaqueBuffer.Destroy(allocator);
        frustumCulledOpaqueDoubleSidedBuffer.Destroy(allocator);
    }
}