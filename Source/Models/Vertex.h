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

#include "Externals/GLM.h"
#include "Util/Util.h"

namespace Models
{
    struct Vertex
    {
        glm::vec3 normal;
        glm::vec2 uv0;
        glm::vec3 tangent;
    };

    #ifdef ENGINE_DEBUG
    static_assert
    (
        sizeof(Vertex) ==
        (
            sizeof(glm::vec3) +
            sizeof(glm::vec2) +
            sizeof(glm::vec3)
        ),
        "Incompatible vertex structure!"
    );
    #endif

    using Index = u32;
}

#endif
