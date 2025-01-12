/*
 * Copyright (c) 2023 - 2025 Rachit
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
    struct Vertex
    {
        Vertex(const glm::vec3& position, const glm::vec2& texCoords, const glm::vec3& normal, const glm::vec3& tangent);

        glm::vec4 position_uvX;
        glm::vec4 normal_uvY;
        glm::vec4 tangent_padf32;
    };

    using Index = u32;
}

#endif
