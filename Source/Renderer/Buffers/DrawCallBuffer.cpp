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

#include "DrawCallBuffer.h"

#include "Util/Log.h"
#include "Vulkan/DebugUtils.h"
#include "Renderer/Buffers/MeshBuffer.h"

namespace Renderer::Buffers
{
    DrawCallBuffer::DrawCallBuffer(VkDevice device, VmaAllocator allocator, Type type)
        : type(type)
    {
        switch (type)
        {
        case Type::CPUToGPU:
            drawCallBuffer = Vk::Buffer
            (
                allocator,
                sizeof(u32) + MAX_MESH_COUNT * sizeof(VkDrawIndexedIndirectCommand),
                VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                VMA_MEMORY_USAGE_AUTO
            );
            break;

        case Type::GPUOnly:
            drawCallBuffer = Vk::Buffer
            (
                allocator,
                sizeof(u32) + MAX_MESH_COUNT * sizeof(VkDrawIndexedIndirectCommand),
                VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                0,
                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
            );

            meshIndexBuffer = Vk::Buffer
            (
                allocator,
                MAX_MESH_COUNT * sizeof(u32),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                0,
                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
            );
            break;
        }

        drawCallBuffer.GetDeviceAddress(device);

        if (meshIndexBuffer.has_value())
        {
            meshIndexBuffer->GetDeviceAddress(device);
        }
    }

    void DrawCallBuffer::WriteDrawCalls
    (
        VmaAllocator allocator,
        const Models::ModelManager& modelManager,
        const std::span<const Renderer::RenderObject> renderObjects
    )
    {
        if (!IsCPUWritable())
        {
            Logger::Error("{}\n", "Draw calls can't be written!");
        }

        std::vector<VkDrawIndexedIndirectCommand> drawCalls = {};

        for (const auto& renderObject : renderObjects)
        {
            for (const auto& mesh : modelManager.GetModel(renderObject.modelID).meshes)
            {
                drawCalls.emplace_back(VkDrawIndexedIndirectCommand{
                    .indexCount    = mesh.indexInfo.count,
                    .instanceCount = 1,
                    .firstIndex    = mesh.indexInfo.offset,
                    .vertexOffset  = static_cast<s32>(mesh.vertexInfo.offset),
                    .firstInstance = 0
                });
            }
        }

        if (drawCalls.size() > MAX_MESH_COUNT)
        {
            Logger::Error("Too many draw calls! [Count={}]\n", drawCalls.size());
        }

        writtenDrawCount = drawCalls.size();

        std::memcpy
        (
            drawCallBuffer.allocationInfo.pMappedData,
            &writtenDrawCount,
            sizeof(u32)
        );

        if (!(drawCallBuffer.memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            Vk::CheckResult(vmaFlushAllocation(
                allocator,
                drawCallBuffer.allocation,
                0,
                sizeof(u32)),
                "Failed to flush allocation!"
            );
        }

        if (!drawCalls.empty())
        {
            std::memcpy
            (
                static_cast<u8*>(drawCallBuffer.allocationInfo.pMappedData) + sizeof(u32),
                drawCalls.data(),
                drawCalls.size() * sizeof(VkDrawIndexedIndirectCommand)
            );

            if (!(drawCallBuffer.memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
            {
                Vk::CheckResult(vmaFlushAllocation(
                    allocator,
                    drawCallBuffer.allocation,
                    sizeof(u32),
                    drawCalls.size() * sizeof(VkDrawIndexedIndirectCommand)),
                    "Failed to flush allocation!"
                );
            }
        }
    }

    bool DrawCallBuffer::IsCPUWritable() const
    {
        return type == Type::CPUToGPU;
    }

    void DrawCallBuffer::Destroy(VmaAllocator allocator)
    {
        drawCallBuffer.Destroy(allocator);

        if (meshIndexBuffer.has_value())
        {
            meshIndexBuffer->Destroy(allocator);
        }
    }
}
