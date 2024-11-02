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

#include "Model.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#include "Vertex.h"
#include "Util/Log.h"
#include "Util/Files.h"
#include "Util/Enum.h"
#include "Util/Util.h"

namespace Models
{
    // Model folder path
    constexpr auto MODEL_ASSETS_DIR = "GFX/";

    Model::Model
    (
        const Vk::Context& context,
        Vk::GeometryBuffer& geometryBuffer,
        Vk::TextureManager& textureManager,
        const std::string_view path
    )
    {
        Logger::Info("Loading model: {}\n", path);

        Assimp::Importer importer = {};

        constexpr u32 ASSIMP_FLAGS = aiProcess_Triangulate              | // Only triangles are supported
                                     aiProcess_FlipUVs                  | // Flip textures
                                     aiProcess_OptimizeMeshes           | // Optimise meshes
                                     aiProcess_OptimizeGraph            | // Optimise scene graph
                                     aiProcess_GenSmoothNormals         | // Generate normals (smooth), if missing
                                     aiProcess_GenUVCoords              | // Generate texture coordinates, if missing
                                     aiProcess_CalcTangentSpace         | // Generate tangents, if missing
                                     aiProcess_JoinIdenticalVertices    | // Join identical vertex groups
                                     aiProcess_ImproveCacheLocality     | // Improves cache efficiency
                                     aiProcess_RemoveRedundantMaterials ; // Removes unnecessary materials

        const std::string assetPath = Engine::Files::GetAssetPath(MODEL_ASSETS_DIR, path);
        const aiScene*    scene     = importer.ReadFile(assetPath, ASSIMP_FLAGS);

        if (scene != nullptr)
        {
            if (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || scene->mRootNode == nullptr)
            {
                Logger::Error("Model Load Failed: {}", importer.GetErrorString());
            }
        }
        else
        {
            Logger::Error("Model Load Failed: {}", importer.GetErrorString());
        }

        assert(scene != nullptr && "clang-tidy is being dumb and hurting my feelings :(");

        ProcessNode
        (
            scene->mRootNode,
            scene,
            Engine::Files::GetDirectory(path),
            context,
            geometryBuffer,
            textureManager
        );

        textureManager.Update(context.device);
    }

    void Model::ProcessNode
    (
        const aiNode* node,
        const aiScene* scene,
        const std::string& directory,
        const Vk::Context& context,
        Vk::GeometryBuffer& geometryBuffer,
        Vk::TextureManager& textureManager
    )
    {
        for (u32 i = 0; i < node->mNumMeshes; ++i)
        {
            meshes.emplace_back(ProcessMesh(
                scene->mMeshes[node->mMeshes[i]],
                scene,
                directory,
                context,
                geometryBuffer,
                textureManager
            ));
        }

        for (u32 i = 0; i < node->mNumChildren; ++i)
        {
            ProcessNode
            (
                node->mChildren[i],
                scene,
                directory,
                context,
                geometryBuffer,
                textureManager
            );
        }
    }

    Mesh Model::ProcessMesh
    (
        const aiMesh* mesh,
        const aiScene* scene,
        const std::string& directory,
        const Vk::Context& context,
        Vk::GeometryBuffer& geometryBuffer,
        Vk::TextureManager& textureManager
    )
    {
        std::vector<Vertex> vertices = {};
        std::vector<Index> indices   = {};

        vertices.reserve(mesh->mNumVertices);
        indices.reserve(mesh->mNumFaces * 3); // Assume triangles

        for (u32 i = 0; i < mesh->mNumVertices; ++i)
        {
            vertices.emplace_back
            (
                glm::ai_cast(mesh->mVertices[i]),
                glm::ai_cast(mesh->mTextureCoords[0][i]),
                glm::ai_cast(mesh->mNormals[i]),
                glm::ai_cast(mesh->mTangents[i])
            );
        }

        for (u32 i = 0; i < mesh->mNumFaces; ++i)
        {
            const aiFace& face = mesh->mFaces[i];
            // Assume triangles
            indices.emplace_back(face.mIndices[0]);
            indices.emplace_back(face.mIndices[1]);
            indices.emplace_back(face.mIndices[2]);
        }

        auto infos = geometryBuffer.LoadData(context, vertices, indices);
        return {
            infos[0],
            infos[1],
            ProcessMaterial(
                mesh,
                scene,
                directory,
                context,
                textureManager
            )
        };
    }

    Material Model::ProcessMaterial
    (
        const aiMesh* mesh,
        const aiScene* scene,
        const std::string& directory,
        const Vk::Context& context,
        Vk::TextureManager& textureManager
    )
    {
        // Default textures
        constexpr auto DEFAULT_TEXTURE_ALBEDO   = "def.png";
        constexpr auto DEFAULT_TEXTURE_NORMAL   = "defNrm.png";
        constexpr auto DEFAULT_TEXTURE_MATERIAL = "def.png";

        aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];

        auto GetTexturePath = [&] (aiTextureType type, const std::string_view defaultPath)
        {
            aiString path;
            mat->GetTexture(type, 0, &path);

            if (path.length > 0)
            {
                return Engine::Files::GetAssetPath(MODEL_ASSETS_DIR + directory, path.C_Str());
            }

            Logger::Warning("Unable to find texture, using default textures! [Type={}]\n", aiTextureTypeToString(type));
            return Engine::Files::GetAssetPath(MODEL_ASSETS_DIR, defaultPath);
        };

        return
        {
            textureManager.AddTexture(context, GetTexturePath(aiTextureType_BASE_COLOR,        DEFAULT_TEXTURE_ALBEDO),   Vk::Texture::Flags::IsSRGB | Vk::Texture::Flags::GenMipmaps),
            textureManager.AddTexture(context, GetTexturePath(aiTextureType_NORMALS,           DEFAULT_TEXTURE_NORMAL),   Vk::Texture::Flags::GenMipmaps),
            textureManager.AddTexture(context, GetTexturePath(aiTextureType_DIFFUSE_ROUGHNESS, DEFAULT_TEXTURE_MATERIAL), Vk::Texture::Flags::GenMipmaps)
        };
    }
}