/*
 *    Copyright 2023 - 2024 Rachit Khandelwal
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "Mesh.h"

#include "Vertex.h"

namespace Models
{
    Mesh::Mesh
    (
        const Vk::Context& context,
        const std::vector<Models::Vertex>& vertices,
        const std::vector<Models::Index>& indices,
        const Models::Material& material
    )
        : vertexBuffer(context, vertices, indices),
          material(material)
    {
    }

    void Mesh::Destroy(VkDevice device, VmaAllocator allocator) const
    {
        vertexBuffer.Destroy(allocator);
        material.Destroy(device, allocator);
    }
}