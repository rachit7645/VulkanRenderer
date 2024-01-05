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

namespace Models
{
    // Model folder path
    constexpr auto MODEL_ASSETS_DIR = "GFX/";

    Model::Model(const std::shared_ptr<Vk::Context>& context, const std::string_view path)
    {
        Logger::Info("Loading model: {}\n", path);

        Assimp::Importer importer = {};

        constexpr u32 ASSIMP_FLAGS = aiProcess_Triangulate              | // Only triangles are supported
                                     aiProcess_FlipUVs                  | // Flip textures
                                     aiProcess_OptimizeMeshes           | // Optimise meshes
                                     aiProcess_OptimizeGraph            | // Optimise scene graph
                                     aiProcess_GenSmoothNormals         | // Generate normals
                                     aiProcess_GenUVCoords              | // Generate texture coordinates
                                     aiProcess_CalcTangentSpace         | // Generate tangents, if missing
                                     aiProcess_JoinIdenticalVertices    | // Join identical vertex groups
                                     aiProcess_ImproveCacheLocality     | // Improves cache efficiency
                                     aiProcess_RemoveRedundantMaterials ; // Removes unnecessary materials

        const aiScene* scene = importer.ReadFile(Engine::Files::GetAssetPath(MODEL_ASSETS_DIR, path), ASSIMP_FLAGS);

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

        // Clang-tidy is dumb
        assert(scene != nullptr && "Why am I giving this an error message");
        ProcessNode(scene->mRootNode, scene, Engine::Files::GetDirectory(path), context);
    }

    void Model::ProcessNode
    (
        aiNode* node,
        const aiScene* scene,
        const std::string& directory,
        const std::shared_ptr<Vk::Context>& context
    )
    {
        for (u32 i = 0; i < node->mNumMeshes; ++i)
        {
            meshes.emplace_back(ProcessMesh(scene->mMeshes[node->mMeshes[i]], scene, directory, context));
        }

        for (u32 i = 0; i < node->mNumChildren; ++i)
        {
            ProcessNode(node->mChildren[i], scene, directory, context);
        }
    }

    Mesh Model::ProcessMesh
    (
        aiMesh* mesh,
        const aiScene* scene,
        const std::string& directory,
        const std::shared_ptr<Vk::Context>& context
    )
    {
        std::vector<Vertex> vertices = {};
        std::vector<Index> indices   = {};

        // Pre-allocate memory
        vertices.reserve(mesh->mNumVertices);
        indices.reserve(mesh->mNumFaces * 3);

        for (u32 i = 0; i < mesh->mNumVertices; ++i)
        {
            // Convert to glm data
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
            // Assume that we only have triangles
            indices.emplace_back(face.mIndices[0]);
            indices.emplace_back(face.mIndices[1]);
            indices.emplace_back(face.mIndices[2]);
        }

        return {context, vertices, indices, ProcessTextures(mesh, scene, directory, context)};
    }

    Material Model::ProcessTextures
    (
        aiMesh* mesh,
        const aiScene* scene,
        const std::string& directory,
        const std::shared_ptr<Vk::Context>& context
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
            Vk::Texture(context, GetTexturePath(aiTextureType_BASE_COLOR,        DEFAULT_TEXTURE_ALBEDO),   Vk::Texture::Flags::IsSRGB | Vk::Texture::Flags::GenMipmaps),
            Vk::Texture(context, GetTexturePath(aiTextureType_NORMALS,           DEFAULT_TEXTURE_NORMAL),   Vk::Texture::Flags::GenMipmaps),
            Vk::Texture(context, GetTexturePath(aiTextureType_DIFFUSE_ROUGHNESS, DEFAULT_TEXTURE_MATERIAL), Vk::Texture::Flags::GenMipmaps)
        };
    }

    std::vector<Models::Material> Model::GetMaterials() const
    {
        // Pre-allocate
        std::vector<Models::Material> materials = {};
        materials.reserve(meshes.size());

        for (const auto& mesh : meshes)
        {
            materials.emplace_back(mesh.material);
        }

        return materials;
    }

    void Model::Destroy(VkDevice device, VmaAllocator allocator)
    {
        for (auto&& mesh : meshes)
        {
            mesh.Destroy(device, allocator);
        }
    }
}