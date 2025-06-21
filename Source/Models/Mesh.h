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

#ifndef MESH_H
#define MESH_H

#include "GPU/Material.h"
#include "Vulkan/GeometryBuffer.h"
#include "GPU/AABB.h"
#include "Vulkan/TextureManager.h"

namespace Models
{
    struct Material
    {
        [[nodiscard]] GPU::Material Convert(const Vk::TextureManager& textureManager) const;

        [[nodiscard]] bool IsAlphaMasked() const;
        [[nodiscard]] bool IsDoubleSided() const;

        void Destroy
        (
            VkDevice device,
            VmaAllocator allocator,
            Vk::MegaSet& megaSet,
            Vk::TextureManager& textureManager,
            Util::DeletionQueue& deletionQueue
        );

        Vk::TextureID albedoID   = 0;
        Vk::TextureID normalID   = 0;
        Vk::TextureID aoRghMtlID = 0;
        Vk::TextureID emmisiveID = 0;

        u32 albedoUVMapID   = 0;
        u32 normalUVMapID   = 0;
        u32 aoRghMtlUVMapID = 0;
        u32 emmisiveUVMapID = 0;

        glm::vec4 albedoFactor     = {1.0f, 1.0f, 1.0f, 1.0f};
        f32       roughnessFactor  = 1.0f;
        f32       metallicFactor   = 1.0f;
        glm::vec3 emmisiveFactor   = {0.0f, 0.0f, 0.0f};
        f32       emmisiveStrength = 0.0f;

        f32 alphaCutOff = 1.0f;

        f32 ior = 1.5f;

        GPU::MaterialFlags flags = GPU::MaterialFlags::None;
    };

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
