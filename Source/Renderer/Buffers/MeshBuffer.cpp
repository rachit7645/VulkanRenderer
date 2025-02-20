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
    constexpr usize MAX_MESH_COUNT = 1 << 16;

    MeshBuffer::MeshBuffer(VkDevice device, VmaAllocator allocator)
    {
        for (usize i = 0; i < buffers.size(); ++i)
        {
            buffers[i] = Vk::Buffer
            (
                allocator,
                MAX_MESH_COUNT * sizeof(Renderer::Mesh),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            buffers[i].GetDeviceAddress(device);

            Vk::SetDebugName(device, buffers[i].handle, fmt::format("MeshBuffer/{}", i));
        }
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
                    glm::uvec3(
                        modelManager.textureManager.GetTextureID(mesh.material.albedo),
                        modelManager.textureManager.GetTextureID(mesh.material.normal),
                        modelManager.textureManager.GetTextureID(mesh.material.aoRghMtl)
                    ),
                    mesh.material.albedoFactor,
                    mesh.material.roughnessFactor,
                    mesh.material.metallicFactor
                );
            }
        }

        std::memcpy
        (
            buffers[FIF].allocationInfo.pMappedData,
            meshes.data(),
            sizeof(Renderer::Mesh) * meshes.size()
        );

        if (!(buffers[FIF].memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            Vk::CheckResult(vmaFlushAllocation(
                allocator,
                buffers[FIF].allocation,
                0,
                sizeof(Renderer::Mesh) * meshes.size()),
                "Failed to flush allocation!"
            );
        }
    }

    void MeshBuffer::Destroy(VmaAllocator allocator)
    {
        for (auto& buffer : buffers)
        {
            buffer.Destroy(allocator);
        }
    }
}
