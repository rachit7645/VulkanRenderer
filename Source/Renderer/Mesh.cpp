#include "Mesh.h"

namespace Renderer
{
    // Vertex data (FIXME: HORRIBLE HARD CODING)
    constexpr std::array<Vertex, 8> VERTICES =
    {
        // Front face
        Vertex(glm::vec3(-1.0f, -1.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f)), // Vertex 0
        Vertex(glm::vec3( 1.0f, -1.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f)), // Vertex 1
        Vertex(glm::vec3( 1.0f,  1.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f)), // Vertex 2
        Vertex(glm::vec3(-1.0f,  1.0f, 1.0f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f)), // Vertex 3

        // Back face
        Vertex(glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f)), // Vertex 4
        Vertex(glm::vec3( 1.0f, -1.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f)), // Vertex 5
        Vertex(glm::vec3( 1.0f,  1.0f, -1.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f)), // Vertex 6
        Vertex(glm::vec3(-1.0f,  1.0f, -1.0f), glm::vec3(1.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f))  // Vertex 7
    };

    // Index data (FIXME: HORRIBLE HARD CODING)
    constexpr std::array<Index, 36> INDICES =
    {
        // Front face
        0, 1, 2,
        2, 3, 0,

        // Right face
        1, 5, 6,
        6, 2, 1,

        // Back face
        5, 4, 7,
        7, 6, 5,

        // Left face
        4, 0, 3,
        3, 7, 4,

        // Top face
        3, 2, 6,
        6, 7, 3,

        // Bottom face
        4, 5, 1,
        1, 0, 4
    };

    Mesh::Mesh(const std::shared_ptr<Vk::Context>& context)
        : vertexBuffer(context, VERTICES, INDICES),
          texture(context, "Assets/GFX/Grass.png")
    {
    }

    void Mesh::DestroyMesh(VkDevice device)
    {
        // Destroy vertex buffer
        vertexBuffer.DestroyBuffer(device);
        // Destroy texture
        texture.Destroy(device);
    }
}