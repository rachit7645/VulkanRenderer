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

#ifndef FORWARD_INSTANCE_H
#define FORWARD_INSTANCE_H

#include <vulkan/vulkan.h>

#include "Externals/GLM.h"
#include "Vulkan/Util.h"
#include "Util/Util.h"

namespace Renderer::Forward
{
    struct Mesh
    {
        glm::mat4  transform;
        glm::mat3  normalMatrix;
        glm::uvec3 textureIDs;
        glm::vec4  albedoFactor;
        f32        roughnessFactor;
        f32        metallicFactor;
    };

    #ifdef ENGINE_DEBUG
    static_assert
    (
        sizeof(Mesh) ==
        (
            sizeof(glm::mat4) +
            sizeof(glm::mat3) +
            sizeof(glm::uvec3) +
            sizeof(glm::vec4) +
            sizeof(f32) +
            sizeof(f32)
        ),
        "Incompatible mesh structure!"
    );
    #endif
}

#endif
