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
        for (usize i = 0; i < m_meshBuffers.size(); ++i)
        {
            m_meshBuffers[i] = Vk::Buffer
            (
                allocator,
                MAX_MESH_COUNT * sizeof(GPU::Mesh),
                0,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            m_meshBuffers[i].GetDeviceAddress(device);

            Vk::SetDebugName(device, m_meshBuffers[i].handle, fmt::format("MeshBuffer/{}", i));
        }

        for (usize i = 0; i < m_instanceBuffers.size(); ++i)
        {
            m_instanceBuffers[i] = Vk::Buffer
            (
                allocator,
                sizeof(u32) + MAX_MESH_COUNT * sizeof(GPU::Instance),
                0,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            constexpr u32 ZERO = 0;

            std::memcpy(m_instanceBuffers[i].hostAddress, &ZERO, sizeof(u32));

            if (!(m_instanceBuffers[i].memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
            {
                Vk::CheckResult(vmaFlushAllocation(
                    allocator,
                    m_instanceBuffers[i].allocation,
                    0,
                    sizeof(u32)),
                    "Failed to flush allocation!"
                );
            }

            m_instanceBuffers[i].GetDeviceAddress(device);

            Vk::SetDebugName(device, m_instanceBuffers[i].handle, fmt::format("InstanceBuffer/{}", i));
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
        ankerl::unordered_dense::set<Models::ModelID> uniqueModelIDs = {};

        for (const auto& renderObject : renderObjects)
        {
            uniqueModelIDs.insert(renderObject.modelID);
        }

        std::vector<GPU::Mesh>                                                 meshes       = {};
        ankerl::unordered_dense::map<std::pair<Models::ModelID, usize>, usize> meshIndexMap = {};

        for (const auto modelID : uniqueModelIDs)
        {
            const auto& model = modelManager.GetModel(modelID);

            for (usize localMeshIndex = 0; localMeshIndex < model.meshes.size(); ++localMeshIndex)
            {
                const auto& mesh = model.meshes[localMeshIndex];

                // (modelID, localMeshIndex) -> globalMeshIndex
                meshIndexMap[{modelID, localMeshIndex}] = meshes.size();

                meshes.emplace_back
                (
                    mesh.surfaceInfo,
                    mesh.material.Convert(modelManager.textureManager),
                    mesh.aabb
                );
            }
        }

        std::vector<GPU::Instance> instances = {};

        for (const auto& renderObject : renderObjects)
        {
            const auto globalTransform = Maths::TransformMatrix
            (
                renderObject.position,
                renderObject.rotation,
                renderObject.scale
            );

            const auto& model = modelManager.GetModel(renderObject.modelID);

            for (usize localMeshIndex = 0; localMeshIndex < model.meshes.size(); ++localMeshIndex)
            {
                const auto& mesh = model.meshes[localMeshIndex];

                const auto transform    = globalTransform * mesh.transform;
                const auto normalMatrix = Maths::NormalMatrix(transform);

                const auto globalMeshIndex = meshIndexMap.at({renderObject.modelID, localMeshIndex});

                instances.emplace_back
                (
                    globalMeshIndex,
                    transform,
                    normalMatrix
                );
            }
        }

        const auto& meshBuffer     = GetCurrentMeshBuffer(frameIndex);
        const auto& instanceBuffer = GetCurrentInstanceBuffer(frameIndex);

        const VkDeviceSize meshCopySize     = meshes.size()    * sizeof(GPU::Mesh);
        const VkDeviceSize instanceCopySize = instances.size() * sizeof(GPU::Instance);

        std::memcpy
        (
            meshBuffer.hostAddress,
            meshes.data(),
            meshCopySize
        );

        const u32 instanceCount = instances.size();

        std::memcpy
        (
            instanceBuffer.hostAddress,
            &instanceCount,
            sizeof(u32)
        );

        std::memcpy
        (
            static_cast<u8*>(instanceBuffer.hostAddress) + sizeof(u32),
            instances.data(),
            instanceCopySize
        );

        if (!(meshBuffer.memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            Vk::CheckResult(vmaFlushAllocation(
                allocator,
                meshBuffer.allocation,
                0,
                meshCopySize),
                "Failed to flush allocation!"
            );
        }

        if (!(instanceBuffer.memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            Vk::CheckResult(vmaFlushAllocation(
                allocator,
                instanceBuffer.allocation,
                0,
                sizeof(u32) + instanceCopySize),
                "Failed to flush allocation!"
            );
        }
    }

    const Vk::Buffer& MeshBuffer::GetCurrentMeshBuffer(usize frameIndex) const
    {
        return m_meshBuffers[frameIndex % m_meshBuffers.size()];
    }

    const Vk::Buffer& MeshBuffer::GetPreviousMeshBuffer(usize frameIndex) const
    {
        return m_meshBuffers[(frameIndex + m_meshBuffers.size() - 1) % m_meshBuffers.size()];
    }

    const Vk::Buffer& MeshBuffer::GetCurrentInstanceBuffer(usize frameIndex) const
    {
        return m_instanceBuffers[frameIndex % m_instanceBuffers.size()];
    }

    const Vk::Buffer& MeshBuffer::GetPreviousInstanceBuffer(usize frameIndex) const
    {
        return m_instanceBuffers[(frameIndex + m_instanceBuffers.size() - 1) % m_instanceBuffers.size()];
    }

    void MeshBuffer::Destroy(VmaAllocator allocator)
    {
        for (auto& buffer : m_meshBuffers)
        {
            buffer.Destroy(allocator);
        }

        for (auto& buffer : m_instanceBuffers)
        {
            buffer.Destroy(allocator);
        }
    }
}
