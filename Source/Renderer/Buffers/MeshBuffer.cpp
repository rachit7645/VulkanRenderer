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

#include "Vulkan/DebugUtils.h"
#include "Util/Maths.h"
#include "Util/Log.h"
#include "GPU/Mesh.h"

namespace Renderer::Buffers
{
    MeshBuffer::MeshBuffer(VkDevice device, VmaAllocator allocator)
    {
        for (usize i = 0; i < m_buffers.size(); ++i)
        {
            m_buffers[i] = Vk::Buffer
            (
                allocator,
                MAX_MESH_COUNT * sizeof(GPU::Mesh),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            m_buffers[i].GetDeviceAddress(device);

            Vk::SetDebugName(device, m_buffers[i].handle, fmt::format("MeshBuffer/{}", i));
        }
    }

    void MeshBuffer::LoadMeshes
    (
        usize frameIndex,
        VmaAllocator allocator,
        const Models::ModelManager& modelManager,
        const std::vector<Renderer::RenderObject>& renderObjects
    )
    {
        std::vector<GPU::Mesh> meshes = {};

        for (const auto& renderObject : renderObjects)
        {
            const auto globalTransform = Maths::TransformMatrix
            (
                renderObject.position,
                renderObject.rotation,
                renderObject.scale
            );

            for (const auto& mesh : modelManager.GetModel(renderObject.modelID).meshes)
            {
                const auto transform    = globalTransform * mesh.transform;
                const auto normalMatrix = Maths::NormalMatrix(transform);

                meshes.emplace_back
                (
                    mesh.surfaceInfo,
                    mesh.material.Convert(modelManager.textureManager),
                    transform,
                    normalMatrix,
                    mesh.aabb
                );
            }
        }

        const auto& buffer = GetCurrentBuffer(frameIndex);

        const VkDeviceSize meshCopySize =  meshes.size() * sizeof(GPU::Mesh);

        std::memcpy
        (
            buffer.allocationInfo.pMappedData,
            meshes.data(),
            meshCopySize
        );

        if (!(buffer.memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            Vk::CheckResult(vmaFlushAllocation(
                allocator,
                buffer.allocation,
                0,
                meshCopySize),
                "Failed to flush allocation!"
            );
        }
    }

    const Vk::Buffer& MeshBuffer::GetCurrentBuffer(usize frameIndex) const
    {
        return m_buffers[frameIndex % m_buffers.size()];
    }

    const Vk::Buffer& MeshBuffer::GetPreviousBuffer(usize frameIndex) const
    {
        return m_buffers[(frameIndex + m_buffers.size() - 1) % m_buffers.size()];
    }

    void MeshBuffer::Destroy(VmaAllocator allocator)
    {
        for (auto& buffer : m_buffers)
        {
            buffer.Destroy(allocator);
        }
    }
}
