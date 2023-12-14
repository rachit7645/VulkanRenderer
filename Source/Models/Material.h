/*
 *    Copyright 2023 Rachit Khandelwal
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

#ifndef MESH_TEXTURES_H
#define MESH_TEXTURES_H

#include "Vulkan/Texture.h"

namespace Models
{
    class Material
    {
    public:
        // Material count
        static constexpr auto MATERIAL_COUNT = 3;

        // Copy textures
        Material
        (
            const Vk::Texture& albedo,
            const Vk::Texture& normal,
            const Vk::Texture& aoRghMtl
        );

        // Equality operator
        bool operator==(const Material& rhs) const;

        // Get texture image views
        [[nodiscard]] std::array<Vk::ImageView, MATERIAL_COUNT> GetViews() const;

        // Destroy textures
        void Destroy(VkDevice device) const;

        // Textures
        Vk::Texture albedo;
        Vk::Texture normal;
        Vk::Texture aoRghMtl;

        // Hashing operator
        struct Hash
        {
            usize operator()(const Material& material) const
            {
                // Combine hashes
                return Vk::Texture::Hash()(material.albedo) ^
                       Vk::Texture::Hash()(material.normal) ^
                       Vk::Texture::Hash()(material.aoRghMtl);
            }
        };

        // Equality comparison operator
        struct Equal
        {
            bool operator()(const Material& lhs, const Material& rhs) const
            {
                // Compare handle
                return lhs == rhs;
            }
        };
    };
}

#endif
