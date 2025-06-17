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

#include "Mesh.h"

namespace Models
{
    GPU::Material Material::Convert(const Vk::TextureManager& textureManager) const
    {
        return GPU::Material
        {
            .albedoID         = textureManager.GetTexture(albedoID).descriptorID,
            .normalID         = textureManager.GetTexture(normalID).descriptorID,
            .aoRghMtlID       = textureManager.GetTexture(aoRghMtlID).descriptorID,
            .emmisiveID       = textureManager.GetTexture(emmisiveID).descriptorID,
            .albedoUVMapID    = albedoUVMapID,
            .normalUVMapID    = normalUVMapID,
            .aoRghMtlUVMapID  = aoRghMtlUVMapID,
            .emmisiveUVMapID  = emmisiveUVMapID,
            .albedoFactor     = albedoFactor,
            .roughnessFactor  = roughnessFactor,
            .metallicFactor   = metallicFactor,
            .emmisiveFactor   = emmisiveFactor,
            .emmisiveStrength = emmisiveStrength,
            .alphaCutOff      = alphaCutOff,
            .ior              = ior,
            .flags            = flags
        };
    }

    bool Material::IsAlphaMasked() const
    {
        return (flags & GPU::MaterialFlags::AlphaMasked) == GPU::MaterialFlags::AlphaMasked;
    }

    bool Material::IsDoubleSided() const
    {
        return (flags & GPU::MaterialFlags::DoubleSided) == GPU::MaterialFlags::DoubleSided;
    }

    void Material::Destroy
    (
        VkDevice device,
        VmaAllocator allocator,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager,
        Util::DeletionQueue& deletionQueue
    )
    {
        textureManager.DestroyTexture
        (
            albedoID,
            device,
            allocator,
            megaSet,
            deletionQueue
        );

        textureManager.DestroyTexture
        (
            normalID,
            device,
            allocator,
            megaSet,
            deletionQueue
        );

        textureManager.DestroyTexture
        (
            aoRghMtlID,
            device,
            allocator,
            megaSet,
            deletionQueue
        );
    }

    void Mesh::Destroy
    (
        VkDevice device,
        VmaAllocator allocator,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager,
        Vk::GeometryBuffer& geometryBuffer,
        Util::DeletionQueue& deletionQueue
    )
    {
        geometryBuffer.Free(surfaceInfo, deletionQueue);

        material.Destroy
        (
            device,
            allocator,
            megaSet,
            textureManager,
            deletionQueue
        );
    }
}
