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

#ifndef MODEL_H
#define MODEL_H

#include <vector>
#include <string_view>

#include <assimp/scene.h>

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
        void ProcessNode
        (
            const aiNode* node,
            const aiScene* scene,
            const std::string& directory,
            const Vk::Context& context,
            Vk::GeometryBuffer& geometryBuffer,
            Vk::TextureManager& textureManager
        );

        [[nodiscard]] Models::Mesh ProcessMesh
        (
            const aiMesh* mesh,
            const aiScene* scene,
            const std::string& directory,
            const Vk::Context& context,
            Vk::GeometryBuffer& geometryBuffer,
            Vk::TextureManager& textureManager
        );

        [[nodiscard]] Material ProcessMaterial
        (
            const aiMesh* mesh,
            const aiScene* scene,
            const std::string& directory,
            const Vk::Context& context,
            Vk::TextureManager& textureManager
        );
    };
}

#endif