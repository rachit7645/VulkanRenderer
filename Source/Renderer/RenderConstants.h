/*
 * Copyright 2023 Rachit Khandelwal
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

namespace Renderer
{
    // Clear color
    constexpr glm::vec4 CLEAR_COLOR = {0.53f, 0.81f, 0.92f, 1.0f};
    // Aspect ratio
    constexpr f32 ASPECT_RATIO = 16.0f / 9.0f;
    // Field of view
    constexpr f32 FOV = glm::radians(70.0f);
    // Planes
    constexpr glm::vec2 PLANES = {0.1f, 200.0f};
}

#endif
