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

#ifndef VERTEX_H
#define VERTEX_H

#include <array>
#include <vulkan/vulkan.h>

#include "Externals/GLM.h"
#include "Util/Util.h"
#include "Vulkan/Util.h"

namespace Models
{
    // TODO: 4 extra bytes are required, might be possible to optimize this
    struct VULKAN_GLSL_DATA Vertex
    {
        Vertex(const glm::vec3& position, const glm::vec2& texCoords, const glm::vec3& normal, const glm::vec3& tangent);

        glm::vec4 attrib0 = {}; // Position + Texture X Coordinate
        glm::vec4 attrib1 = {}; // Texture Y Coordinate + Normal
        glm::vec4 attrib2 = {}; // Tangent + Padding
    };

    using Index = u32;
}

#endif
