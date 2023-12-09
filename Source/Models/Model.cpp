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
                                 aiProcess_JoinIdenticalVertices    | // Join identical vertex groups
                                 aiProcess_ImproveCacheLocality     | // Improves cache efficiency
                                 aiProcess_RemoveRedundantMaterials ; // Removes unnecessary materials

    // Model prefix path
    constexpr auto MODEL_ASSETS_DIR = "GFX/";
    // Default textures
    constexpr auto DEFAULT_TEXTURE_ALBEDO = "Grass.png";

    Model::Model(const std::shared_ptr<Vk::Context>& context, const std::string_view path)
    {
        // Log
        Logger::Info("Loading model: {}\n", path);

        // Get importer
        Assimp::Importer importer = {};

        // Load scene
        const aiScene* scene = importer.ReadFile(Engine::Files::GetAssetPath(MODEL_ASSETS_DIR, path), ASSIMP_FLAGS);
        // Check if scene is OK
        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
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
                glm::ai_cast(mesh->mNormals[i])
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

    Vk::Texture Model::ProcessTextures
    (
        aiMesh* mesh,
        const aiScene* scene,
        const std::string& directory,
        const std::shared_ptr<Vk::Context>& context
    )
    {
        // Current materials
        aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];

        // Utility lambda
        auto GetTexturePath = [&mat, &mesh, &scene, &directory] (aiTextureType type)
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
            else
            {
                // Log
                Logger::Warning
                (
                    "Unable to find texture path, using default textures! [Type={}] [mesh={}] [scene={}]\n",
                    aiTextureTypeToString(type),
                    reinterpret_cast<const void*>(mesh),
                    reinterpret_cast<const void*>(scene)
                );
                // Return
                return Engine::Files::GetAssetPath(MODEL_ASSETS_DIR, DEFAULT_TEXTURE_ALBEDO);
            }
        };

        // Albedo
        return {context, GetTexturePath(aiTextureType_BASE_COLOR)};
    }

    void Model::Destroy(VkDevice device)
    {
        // Destroy all meshes
        for (auto&& mesh : meshes)
        {
            mesh.DestroyMesh(device);
        }
    }

    std::vector<Vk::ImageView> Model::GetTextureViews() const
    {
        // Pre-allocate vector
        std::vector<Vk::ImageView> imageViews = {};
        imageViews.reserve(meshes.size());

        // Add each view to vector
        for (const auto& mesh : meshes)
        {
            imageViews.emplace_back(mesh.texture.imageView);
        }

        // Return
        return imageViews;
    }
}