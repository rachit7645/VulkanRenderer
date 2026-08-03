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

#include "Material.h"

namespace Models
{
    GPU::Material Material::Convert(Vk::TextureManager& textureManager) const
    {
        const u16 packedUVIDs = (albedoUVMapID   & 0xFu)       |
                                (normalUVMapID   & 0xFu) << 4  |
                                (aoRghMtlUVMapID & 0xFu) << 8  |
                                (emissiveUVMapID & 0xFu) << 12;

        return GPU::Material
        {
            .albedoID         = textureManager.GetTexture(albedoID).descriptorID,
            .normalID         = textureManager.GetTexture(normalID).descriptorID,
            .aoRghMtlID       = textureManager.GetTexture(aoRghMtlID).descriptorID,
            .emissiveID       = textureManager.GetTexture(emissiveID).descriptorID,
            .albedoFactor     = albedoFactor,
            .roughnessFactor  = roughnessFactor,
            .metallicFactor   = metallicFactor,
            .emissiveFactor   = emissiveFactor,
            .emissiveStrength = emissiveStrength,
            .alphaCutOff      = alphaCutOff,
            .ior              = ior,
            .packedUVIDs      = packedUVIDs,
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
        Engine::DeletionQueue& deletionQueue
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

        textureManager.DestroyTexture
        (
            emissiveID,
            device,
            allocator,
            megaSet,
            deletionQueue
        );
    }
}