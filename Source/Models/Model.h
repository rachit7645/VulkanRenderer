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

#ifndef MODEL_H
#define MODEL_H

#include <vector>
#include <string_view>

#include <fastgltf/types.hpp>

#include "Mesh.h"
#include "Vulkan/Context.h"
#include "Vulkan/TextureManager.h"
#include "Vulkan/GeometryBuffer.h"

namespace Models
{
    class Model
    {
    public:
        Model
        (
            const Vk::Context& context,
            Vk::GeometryBuffer& geometryBuffer,
            Vk::TextureManager& textureManager,
            const std::string_view path
        );

        std::vector<Models::Mesh> meshes;
    private:
        usize LoadTexture
        (
            const Vk::Context& context,
            Vk::TextureManager& textureManager,
            const std::string_view path,
            const std::string& directory,
            const fastgltf::Asset& asset,
            usize textureIndex,
            Vk::Texture::Flags flags
        );
    };
}

#endif