#ifndef VERTEX_H
#define VERTEX_H

#include <array>
#include <vulkan/vulkan.h>

#include "Externals/GLM.h"
#include "Util/Util.h"

namespace Renderer
{
    struct Vertex
    {
        // Constants
        static constexpr usize VERTEX_NUM_ATTRIBS = 2;
        // Usings
        using VertexAttribs = std::array<VkVertexInputAttributeDescription, VERTEX_NUM_ATTRIBS>;

        // Constructor
        constexpr Vertex(glm::vec2 position, glm::vec3 color)
            : position(position),
              color(color) {}

        // Vertex data
        glm::vec2 position = {};
        glm::vec3 color    = {};

        // Get binding description
        static VkVertexInputBindingDescription GetBindingDescription();
        static VertexAttribs GetVertexAttribDescription();
    };
}

#endif
