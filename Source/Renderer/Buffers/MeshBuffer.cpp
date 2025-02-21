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

#include "MeshBuffer.h"

#include "Renderer/Mesh.h"
#include "Vulkan/DebugUtils.h"
#include "Util/Maths.h"
#include "Util/Log.h"

namespace Renderer::Buffers
{
    MeshBuffer::MeshBuffer(VkDevice device, VmaAllocator allocator)
    {
        for (usize i = 0; i < meshBuffers.size(); ++i)
        {
            meshBuffers[i] = Vk::Buffer
            (
                allocator,
                MAX_MESH_COUNT * sizeof(Renderer::Mesh),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            meshBuffers[i].GetDeviceAddress(device);

            Vk::SetDebugName(device, meshBuffers[i].handle, fmt::format("MeshBuffer/{}", i));
        }

        visibleMeshBuffer = Vk::Buffer
        (
            allocator,
            MAX_MESH_COUNT * sizeof(u32),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        );

        visibleMeshBuffer.GetDeviceAddress(device);

        Vk::SetDebugName(device, visibleMeshBuffer.handle, "VisibleMeshBuffer");
    }

    void MeshBuffer::LoadMeshes
    (
        usize FIF,
        VmaAllocator allocator,
        const Models::ModelManager& modelManager,
        const std::vector<Renderer::RenderObject>& renderObjects
    )
    {
        std::vector<Renderer::Mesh> meshes = {};

        for (const auto& renderObject : renderObjects)
        {
            const auto globalTransform = Maths::CreateTransformMatrix
            (
                renderObject.position,
                renderObject.rotation,
                renderObject.scale
            );

            for (const auto& mesh : modelManager.GetModel(renderObject.modelID).meshes)
            {
                const auto transform    = globalTransform * mesh.transform;
                const auto normalMatrix = Maths::CreateNormalMatrix(transform);

                meshes.emplace_back(
                    transform,
                    normalMatrix,
                    mesh.material,
                    mesh.aabb.Transform(transform)
                );
            }
        }

        std::memcpy
        (
            meshBuffers[FIF].allocationInfo.pMappedData,
            meshes.data(),
            sizeof(Renderer::Mesh) * meshes.size()
        );

        if (!(meshBuffers[FIF].memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            Vk::CheckResult(vmaFlushAllocation(
                allocator,
                meshBuffers[FIF].allocation,
                0,
                sizeof(Renderer::Mesh) * meshes.size()),
                "Failed to flush allocation!"
            );
        }
    }

    void MeshBuffer::Destroy(VmaAllocator allocator)
    {
        for (auto& buffer : meshBuffers)
        {
            buffer.Destroy(allocator);
        }

        visibleMeshBuffer.Destroy(allocator);
    }
}
