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

        culledDrawCallBuffer = DrawCallBuffer(device, allocator, DrawCallBuffer::Type::GPUOnly);

        Vk::SetDebugName(device, culledDrawCallBuffer.drawCallBuffer.handle,  "IndirectBuffer/DrawCallBuffer/CulledDrawCalls");
        Vk::SetDebugName(device, culledDrawCallBuffer.meshIndexBuffer.handle, "IndirectBuffer/DrawCallBuffer/CulledMeshIndices");
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

        culledDrawCallBuffer.Destroy(allocator);
    }
}