#include "Model.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#include "Vertex.h"
#include "Util/Log.h"
#include "Util/Files.h"

namespace Models
{
    // Assimp flags
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

    // Model prefix path
    constexpr auto MODEL_ASSETS_DIR = "GFX/";

    Model::Model(const std::shared_ptr<Vk::Context>& context, const std::string_view path)
    {
        // Log
        Logger::Info("Loading model: {}\n", path);

        // Get importer
        Assimp::Importer importer = {};

        // Load scene
        const aiScene* scene = importer.ReadFile(Engine::Files::GetAssetPath(MODEL_ASSETS_DIR, path), ASSIMP_FLAGS);

        // Check if scene is OK
        if (scene != nullptr)
        {
            if (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || scene->mRootNode == nullptr)
            {
                // Log error
                Logger::Error("Model Load Failed: {}", importer.GetErrorString());
            }
        }
        else
        {
            // Log error
            Logger::Error("Model Load Failed: {}", importer.GetErrorString());
        }

        // Process scene nodes
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
        // Iterate over all the node's meshes
        for (u32 i = 0; i < node->mNumMeshes; ++i)
        {
            // Add meshes
            meshes.emplace_back(ProcessMesh(scene->mMeshes[node->mMeshes[i]], scene, directory, context));
        }

        // Iterate over all the child nodes
        for (u32 i = 0; i < node->mNumChildren; ++i)
        {
            // Process nodes recursively
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
        // Vertex data (packed)
        std::vector<Vertex> vertices;
        // Index data
        std::vector<Index> indices;

        // Pre-allocate memory
        vertices.reserve(mesh->mNumVertices);
        indices.reserve(mesh->mNumFaces * 3);

        // For all vertices
        for (u32 i = 0; i < mesh->mNumVertices; ++i)
        {
            // Convert to GPU friendly data
            vertices.emplace_back
            (
                glm::ai_cast(mesh->mVertices[i]),
                glm::ai_cast(mesh->mTextureCoords[0][i]),
                glm::ai_cast(mesh->mNormals[i]),
                glm::ai_cast(mesh->mTangents[i])
            );
        }

        // For all faces
        for (u32 i = 0; i < mesh->mNumFaces; ++i)
        {
            // Get faces
            const aiFace& face = mesh->mFaces[i];
            // Store indices
            indices.emplace_back(face.mIndices[0]);
            indices.emplace_back(face.mIndices[1]);
            indices.emplace_back(face.mIndices[2]);
        }

        // Return mesh
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

        // Current materials
        aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];

        // Utility lambda
        auto GetTexturePath = [&] (aiTextureType type, const std::string_view defaultPath)
        {
            // Path variable
            aiString path;
            // Get texture
            mat->GetTexture(type, 0, &path);

            // If texture is available
            if (path.length > 0)
            {
                // Found path
                return Engine::Files::GetAssetPath(MODEL_ASSETS_DIR + directory, path.C_Str());
            }

            // If texture not found
            Logger::Warning
            (
                "Unable to find texture path, using default textures! [Type={}] [mesh={}] [scene={}]\n",
                aiTextureTypeToString(type),
                reinterpret_cast<const void*>(mesh),
                reinterpret_cast<const void*>(scene)
            );
            // Return
            return Engine::Files::GetAssetPath(MODEL_ASSETS_DIR, defaultPath);
        };

        // Mesh textures
        return
        {
            Vk::Texture(context, GetTexturePath(aiTextureType_BASE_COLOR,        DEFAULT_TEXTURE_ALBEDO),   Vk::Texture::Flags::IsSRGB | Vk::Texture::Flags::GenMipmaps),
            Vk::Texture(context, GetTexturePath(aiTextureType_NORMALS,           DEFAULT_TEXTURE_NORMAL),   Vk::Texture::Flags::GenMipmaps),
            Vk::Texture(context, GetTexturePath(aiTextureType_DIFFUSE_ROUGHNESS, DEFAULT_TEXTURE_MATERIAL), Vk::Texture::Flags::GenMipmaps)
        };
    }

    std::vector<Models::Material> Model::GetMaterials() const
    {
        // Materials
        std::vector<Models::Material> materials = {};
        materials.reserve(meshes.size());

        // For each mesh
        for (const auto& mesh : meshes)
        {
            // Add material
            materials.emplace_back(mesh.material);
        }

        // Return
        return materials;
    }

    void Model::Destroy(VkDevice device)
    {
        // Destroy all meshes
        for (auto&& mesh : meshes)
        {
            mesh.Destroy(device);
        }
    }
}