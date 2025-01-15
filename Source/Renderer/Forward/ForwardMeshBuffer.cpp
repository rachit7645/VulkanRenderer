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

#include "ForwardMeshBuffer.h"
#include "ForwardMesh.h"

#include "Util/Maths.h"

namespace Renderer::Forward
{
    constexpr usize MAX_MESH_COUNT = 1 << 16;

    MeshBuffer::MeshBuffer(VkDevice device, VmaAllocator allocator)
    {
        for (auto& buffer : buffers)
        {
            buffer = Vk::Buffer
            (
                allocator,
                static_cast<u32>(MAX_MESH_COUNT * sizeof(Forward::Mesh)),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            buffer.GetDeviceAddress(device);
        }
    }

    void MeshBuffer::LoadMeshes
    (
        usize FIF,
        const Vk::TextureManager& textureManager,
        const Models::ModelManager& modelManager,
        const std::vector<Renderer::RenderObject>& renderObjects
    )
    {
        std::vector<Forward::Mesh> meshes = {};

        for (const auto& renderObject : renderObjects)
        {
            const auto transform = Maths::CreateTransformMatrix(renderObject.position, renderObject.rotation, renderObject.scale);
            auto normalMatrix    = glm::mat4(Maths::CreateNormalMatrix(transform));

            for (const auto& mesh : modelManager.GetModel(renderObject.modelID).meshes)
            {
                normalMatrix[0].w = mesh.material.roughnessFactor;
                normalMatrix[1].w = mesh.material.metallicFactor;
                normalMatrix[3]   = mesh.material.albedoFactor;

                meshes.emplace_back(
                    transform,
                    normalMatrix,
                    glm::uvec4(
                        textureManager.GetID(mesh.material.albedo),
                        textureManager.GetID(mesh.material.normal),
                        textureManager.GetID(mesh.material.aoRghMtl),
                        69
                    ),
                    mesh.vertexBuffer.vertexBuffer.deviceAddress
                );
            }
        }

        std::memcpy
        (
            buffers[FIF].allocInfo.pMappedData,
            meshes.data(),
            sizeof(Forward::Mesh) * meshes.size()
        );
    }

    void MeshBuffer::Destroy(VmaAllocator allocator)
    {
        for (auto& buffer : buffers)
        {
            buffer.Destroy(allocator);
        }
    }
}
