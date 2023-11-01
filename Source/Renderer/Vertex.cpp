#include "Vertex.h"

namespace Renderer
{
    VkVertexInputBindingDescription Vertex::GetBindingDescription()
    {
        return
        {
            .binding   = 0,
            .stride    = static_cast<u32>(sizeof(Vertex)),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        };
    }

    Vertex::VertexAttribs Vertex::GetVertexAttribDescription()
    {
        // Attribute data
        VertexAttribs attribs = {};

        // First attrib (position)
        attribs[0] =
        {
            .location = 0,
            .binding  = 0,
            .format   = VK_FORMAT_R32G32_SFLOAT,
            .offset   = offsetof(Vertex, position)
        };

        // First attrib (position)
        attribs[1] =
        {
            .location = 1,
            .binding  = 0,
            .format   = VK_FORMAT_R32G32B32_SFLOAT,
            .offset   = offsetof(Vertex, color)
        };

        return attribs;
    }
}