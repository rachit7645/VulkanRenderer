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
            Vk::MegaSet& megaSet,
            Vk::GeometryBuffer& geometryBuffer,
            Vk::TextureManager& textureManager,
            const std::string_view path
        );

        std::vector<Models::Mesh> meshes;
    private:
        void ProcessScenes
        (
            const Vk::Context& context,
            Vk::MegaSet& megaSet,
            Vk::GeometryBuffer& geometryBuffer,
            Vk::TextureManager& textureManager,
            const std::string& directory,
            const fastgltf::Asset& asset
        );

        void ProcessNode
        (
            const Vk::Context& context,
            Vk::MegaSet& megaSet,
            Vk::GeometryBuffer& geometryBuffer,
            Vk::TextureManager& textureManager,
            const std::string& directory,
            const fastgltf::Asset& asset,
            usize nodeIndex,
            glm::mat4 nodeMatrix
        );

        void LoadMesh
        (
            const Vk::Context& context,
            Vk::MegaSet& megaSet,
            Vk::GeometryBuffer& geometryBuffer,
            Vk::TextureManager& textureManager,
            const std::string& directory,
            const fastgltf::Asset& asset,
            const fastgltf::Mesh& mesh,
            const glm::mat4& nodeMatrix
        );

        glm::mat4 GetTranformMatrix(const fastgltf::Node& node, const glm::mat4& base = glm::identity<glm::mat4>());

        const fastgltf::Accessor& GetAccesor
        (
            const fastgltf::Asset& asset,
            const fastgltf::Primitive& primitive,
            const std::string_view attribute,
            fastgltf::AccessorType type
        );

        usize LoadTexture
        (
            const Vk::Context& context,
            Vk::MegaSet& megaSet,
            Vk::TextureManager& textureManager,
            const std::string& directory,
            const fastgltf::Asset& asset,
            usize textureIndex
        );
    };
}

#endif