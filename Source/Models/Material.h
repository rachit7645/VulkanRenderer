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

#ifndef MATERIAL_H
#define MATERIAL_H

#include "GPU/Material.h"
#include "Vulkan/TextureManager.h"

namespace Models
{
    struct Material
    {
        [[nodiscard]] GPU::Material Convert(Vk::TextureManager& textureManager) const;

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

        Vk::TextureID albedoID   = {};
        Vk::TextureID normalID   = {};
        Vk::TextureID aoRghMtlID = {};
        Vk::TextureID emissiveID = {};

        u8 albedoUVMapID   = 0;
        u8 normalUVMapID   = 0;
        u8 aoRghMtlUVMapID = 0;
        u8 emissiveUVMapID = 0;

        glm::vec4 albedoFactor     = {1.0f, 1.0f, 1.0f, 1.0f};
        f32       roughnessFactor  = 1.0f;
        f32       metallicFactor   = 1.0f;
        glm::vec3 emissiveFactor   = {0.0f, 0.0f, 0.0f};
        f32       emissiveStrength = 0.0f;

        f32 alphaCutOff = 1.0f;

        f32 ior = 1.5f;

        GPU::MaterialFlags flags = GPU::MaterialFlags::None;
    };
}

#endif