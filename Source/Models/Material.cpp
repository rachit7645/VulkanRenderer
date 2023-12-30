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

#include "Material.h"

namespace Models
{
    Material::Material
    (
        const Vk::Texture& albedo,
        const Vk::Texture& normal,
        const Vk::Texture& aoRghMtl
    )
        : albedo(albedo),
          normal(normal),
          aoRghMtl(aoRghMtl)
    {
    }

    bool Material::operator==(const Material& rhs) const
    {
        // Return
        return albedo == rhs.albedo &&
               normal == rhs.normal &&
               aoRghMtl == rhs.aoRghMtl;
    }

    std::array<Vk::ImageView, Material::MATERIAL_COUNT> Material::GetViews() const
    {
        // Return
        return
        {
            albedo.imageView,
            normal.imageView,
            aoRghMtl.imageView
        };
    }

    void Material::Destroy(VkDevice device, VmaAllocator allocator) const
    {
        // Destroy textures
        albedo.Destroy(device, allocator);
        normal.Destroy(device, allocator);
        aoRghMtl.Destroy(device, allocator);
    }
}