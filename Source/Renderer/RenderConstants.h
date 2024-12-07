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

#ifndef RENDER_CONSTANTS_H
#define RENDER_CONSTANTS_H

#include "Externals/GLM.h"
#include "Util/Util.h"

namespace Renderer
{
    // Clear color
    constexpr glm::vec4 CLEAR_COLOR = {0.2474f, 0.62902f, 0.8324f, 1.0f};
    // Default field of view
    constexpr f32 DEFAULT_FOV = glm::radians(80.0f);
    // Near and far plane
    constexpr glm::vec2 PLANES = {0.1f, 2048.0f};
    // World Up direction
    constexpr glm::vec3 WORLD_UP = {0.0f, 1.0f, 0.0f};
}

#endif
