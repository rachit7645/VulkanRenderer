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
        Vk::TextureID albedo;
        Vk::TextureID normal;
        Vk::TextureID aoRghMtl;

        glm::vec4 albedoFactor;
        f32       roughnessFactor;
        f32       metallicFactor;

        f32 alphaCutOff;

        GPU::MaterialFlags flags;

        [[nodiscard]] GPU::Material Convert(const Vk::TextureManager& textureManager) const
        {
            return GPU::Material
            {
                .albedo          = textureManager.GetTextureInfo(albedo).descriptorID,
                .normal          = textureManager.GetTextureInfo(normal).descriptorID,
                .aoRghMtl        = textureManager.GetTextureInfo(aoRghMtl).descriptorID,
                .albedoFactor    = albedoFactor,
                .roughnessFactor = roughnessFactor,
                .metallicFactor  = metallicFactor,
                .alphaCutOff     = alphaCutOff,
                .flags           = flags
            };
        }

        [[nodiscard]] bool IsAlphaMasked() const
        {
            return (flags & GPU::MaterialFlags::AlphaMasked) == GPU::MaterialFlags::AlphaMasked;
        }

        [[nodiscard]] bool IsDoubleSided() const
        {
            return (flags & GPU::MaterialFlags::DoubleSided) == GPU::MaterialFlags::DoubleSided;
        }
    };

    struct Mesh
    {
        GPU::SurfaceInfo surfaceInfo = {};
        Models::Material material    = {};
        glm::mat4        transform   = glm::identity<glm::mat4>();
        GPU::AABB        aabb        = {};
    };
}

#endif
