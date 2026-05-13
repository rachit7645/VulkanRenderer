/*
 * Copyright (c) 2023 - 2026 Rachit
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

#ifndef MESH_H
#define MESH_H

#include "Material.h"
#include "Vulkan/GeometryBuffer.h"
#include "GPU/Surface.h"
#include "GPU/AABB.h"

namespace Models
{
    struct Mesh
    {
        void Destroy
        (
            VkDevice device,
            VmaAllocator allocator,
            Vk::MegaSet& megaSet,
            Vk::TextureManager& textureManager,
            Vk::GeometryBuffer& geometryBuffer,
            Util::DeletionQueue& deletionQueue
        );

        GPU::SurfaceInfo surfaceInfo = {};
        Models::Material material    = {};
        glm::mat4        transform   = glm::identity<glm::mat4>();
        GPU::AABB        aabb        = {};
    };
}

#endif
