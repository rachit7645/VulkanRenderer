/*
 *    Copyright 2023 Rachit Khandelwal
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
            .format   = VK_FORMAT_R32G32B32_SFLOAT,
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