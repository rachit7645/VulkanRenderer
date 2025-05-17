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
        : frustumCulledBuffers(device, allocator)
    {
        for (usize i = 0; i < writtenDrawCallBuffers.size(); ++i)
        {
            writtenDrawCallBuffers[i] = DrawCallBuffer(device, allocator, DrawCallBuffer::Type::CPUToGPU);

            Vk::SetDebugName(device, writtenDrawCallBuffers[i].drawCallBuffer.handle, fmt::format("IndirectBuffer/DrawCallBuffer/DrawCalls/{}", i));
        }

        Vk::SetDebugName(device, frustumCulledBuffers.opaqueBuffer.drawCallBuffer.handle,                   "IndirectBuffer/DrawCallBuffer/FrustumCulled/Opaque/DrawCalls");
        Vk::SetDebugName(device, frustumCulledBuffers.opaqueBuffer.meshIndexBuffer->handle,                 "IndirectBuffer/DrawCallBuffer/FrustumCulled/Opaque/MeshIndices");
        Vk::SetDebugName(device, frustumCulledBuffers.opaqueDoubleSidedBuffer.drawCallBuffer.handle,        "IndirectBuffer/DrawCallBuffer/FrustumCulled/Opaque/DoubleSided/DrawCalls");
        Vk::SetDebugName(device, frustumCulledBuffers.opaqueDoubleSidedBuffer.meshIndexBuffer->handle,      "IndirectBuffer/DrawCallBuffer/FrustumCulled/Opaque/DoubleSided/MeshIndices");
        Vk::SetDebugName(device, frustumCulledBuffers.alphaMaskedBuffer.drawCallBuffer.handle,              "IndirectBuffer/DrawCallBuffer/FrustumCulled/AlphaMasked/DrawCalls");
        Vk::SetDebugName(device, frustumCulledBuffers.alphaMaskedBuffer.meshIndexBuffer->handle,            "IndirectBuffer/DrawCallBuffer/FrustumCulled/AlphaMasked/MeshIndices");
        Vk::SetDebugName(device, frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.drawCallBuffer.handle,   "IndirectBuffer/DrawCallBuffer/FrustumCulled/AlphaMasked/DoubleSided/DrawCalls");
        Vk::SetDebugName(device, frustumCulledBuffers.alphaMaskedDoubleSidedBuffer.meshIndexBuffer->handle, "IndirectBuffer/DrawCallBuffer/FrustumCulled/AlphaMasked/DoubleSided/MeshIndices");
    }

    IndirectBuffer::CulledBuffers::CulledBuffers(VkDevice device, VmaAllocator allocator)
        : opaqueBuffer(device, allocator, DrawCallBuffer::Type::GPUOnly),
          opaqueDoubleSidedBuffer(device, allocator, DrawCallBuffer::Type::GPUOnly),
          alphaMaskedBuffer(device, allocator, DrawCallBuffer::Type::GPUOnly),
          alphaMaskedDoubleSidedBuffer(device, allocator, DrawCallBuffer::Type::GPUOnly)
    {
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

    void IndirectBuffer::CulledBuffers::Destroy(VmaAllocator allocator)
    {
        opaqueBuffer.Destroy(allocator);
        opaqueDoubleSidedBuffer.Destroy(allocator);
        alphaMaskedBuffer.Destroy(allocator);
        alphaMaskedDoubleSidedBuffer.Destroy(allocator);
    }

    void IndirectBuffer::Destroy(VmaAllocator allocator)
    {
        for (auto& buffer : writtenDrawCallBuffers)
        {
            buffer.Destroy(allocator);
        }

        frustumCulledBuffers.Destroy(allocator);
    }
}