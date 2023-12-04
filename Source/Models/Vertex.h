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

#ifndef VERTEX_H
#define VERTEX_H

#include <array>
#include <vulkan/vulkan.h>

#include "Externals/GLM.h"
#include "Util/Util.h"

namespace Models
{
    struct Vertex
    {
        // Constants
        static constexpr usize VERTEX_NUM_ATTRIBS = 3;
        // Usings
        using VertexAttribs = std::array<VkVertexInputAttributeDescription, VERTEX_NUM_ATTRIBS>;

        // Constructor
        constexpr Vertex(const glm::vec3& position, const glm::vec2& texCoords, const glm::vec3& normal)
            : position(position),
              texCoords(texCoords),
              normal(normal) {}

        // Vertex data
        glm::vec3 position  = {};
        glm::vec2 texCoords = {};
        glm::vec3 normal    = {};

        // Get binding description
        static VkVertexInputBindingDescription GetBindingDescription();
        static VertexAttribs GetVertexAttribDescription();
    };

    // Index data
    using Index = u32;
}

#endif