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

#ifndef MESH_TEXTURES_H
#define MESH_TEXTURES_H

#include "Vulkan/Texture.h"

namespace Models
{
    class Material
    {
    public:
        // Update if this struct changes
        static constexpr auto MATERIAL_COUNT = 3;

        Material
        (
            const Vk::Texture& albedo,
            const Vk::Texture& normal,
            const Vk::Texture& aoRghMtl
        );

        bool operator==(const Material& rhs) const;

        [[nodiscard]] std::array<Vk::ImageView, MATERIAL_COUNT> GetViews() const;

        void Destroy(VkDevice device, VmaAllocator allocator) const;

        // Textures
        Vk::Texture albedo;
        Vk::Texture normal;
        Vk::Texture aoRghMtl;
    };
}

// Please don't nuke me for this
namespace std
{
    template <>
    struct hash<Models::Material>
    {
        usize operator()(const Models::Material& material) const noexcept
        {
            return std::hash<Vk::Texture>()(material.albedo) ^
                   std::hash<Vk::Texture>()(material.normal) ^
                   std::hash<Vk::Texture>()(material.aoRghMtl);
        }
    };
}

#endif
