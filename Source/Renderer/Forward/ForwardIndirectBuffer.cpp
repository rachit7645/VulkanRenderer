//    Copyright 2023 - 2024 Rachit Khandelwal
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.

#include "ForwardIndirectBuffer.h"

namespace Renderer::Forward
{
    constexpr usize MAX_DRAW_COUNT = 1 << 16;

    IndirectBuffer::IndirectBuffer(VmaAllocator allocator)
    {
        for (auto& buffer : buffers)
        {
            buffer = Vk::Buffer
            (
                allocator,
                static_cast<u32>(MAX_DRAW_COUNT * sizeof(VkDrawIndexedIndirectCommand)),
                VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                VMA_MEMORY_USAGE_AUTO
            );
        }
    }

    u32 IndirectBuffer::WriteDrawCalls
    (
        usize FIF,
        const Models::ModelManager& modelManager,
        const std::vector<Renderer::RenderObject>& renderObjects
    )
    {
        std::vector<VkDrawIndexedIndirectCommand> drawCalls = {};

        for (const auto& renderObject : renderObjects)
        {
            for (const auto& mesh : modelManager.GetModel(renderObject.modelID).meshes)
            {
                drawCalls.emplace_back(VkDrawIndexedIndirectCommand{
                    .indexCount    = mesh.indexData.count,
                    .instanceCount = 1,
                    .firstIndex    = mesh.indexData.offset,
                    .vertexOffset  = static_cast<s32>(mesh.vertexData.offset),
                    .firstInstance = 0
                });
            }
        }

        std::memcpy
        (
            buffers[FIF].allocInfo.pMappedData,
            drawCalls.data(),
            sizeof(VkDrawIndexedIndirectCommand) * drawCalls.size()
        );

        return static_cast<u32>(drawCalls.size());
    }

    void IndirectBuffer::Destroy(VmaAllocator allocator)
    {
        for (auto& buffer : buffers)
        {
            buffer.Destroy(allocator);
        }
    }
}