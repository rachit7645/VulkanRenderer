#ifndef MODEL_H
#define MODEL_H

#include <memory>
#include <vector>
#include <string_view>

#include <assimp/scene.h>

#include "Mesh.h"
#include "Vulkan/Context.h"
#include "Util/Util.h"

namespace Models
{
    class Model
    {
    public:
        // Main constructor
        Model(const std::shared_ptr<Vk::Context>& context, const std::string_view path);
        // Destroy model
        void Destroy(VkDevice device);
        // Data
        std::vector<Models::Mesh> meshes;
    private:
        // Process each node recursively
        void ProcessNode
        (
            aiNode* node,
            const aiScene* scene,
            const std::string& directory,
            const std::shared_ptr<Vk::Context>& context
        );
        // Process each mesh in the node
        Models::Mesh ProcessMesh
        (
            aiMesh* mesh,
            const aiScene* scene,
            const std::string& directory,
            const std::shared_ptr<Vk::Context>& context
        );
        // Process each texture in the mesh
        Vk::Texture ProcessTextures
        (
            aiMesh* mesh,
            const aiScene* scene,
            const std::string& directory,
            const std::shared_ptr<Vk::Context>& context
        );
    };
}

#endif