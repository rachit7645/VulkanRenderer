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

#ifndef DIRECTIONAL_LIGHT_H
#define DIRECTIONAL_LIGHT_H

#include "Externals/GLM.h"
#include "Util/Util.h"

namespace Renderer::Objects
{
    constexpr auto MAX_DIR_LIGHT_COUNT   = 1u;
    constexpr auto MAX_POINT_LIGHT_COUNT = 16u;
    constexpr auto MAX_SPOT_LIGHT_COUNT  = 16u;

    struct DirLight
    {
        glm::vec3 position       = {0.0f, 0.0f, 0.0f};
        glm::vec3 color          = {0.0f, 0.0f, 0.0f};
        glm::vec3 intensity      = {0.0f, 0.0f, 0.0f};
        u32       shadowMapIndex = 0;
    };

    struct PointLight
    {
        glm::vec3 position       = {0.0f, 0.0f, 0.0f};
        glm::vec3 color          = {0.0f, 0.0f, 0.0f};
        glm::vec3 intensity      = {0.0f, 0.0f, 0.0f};
        glm::vec3 attenuation    = {0.0f, 0.0f, 0.0f};
        u32       shadowMapIndex = 0;
    };

    struct SpotLight
    {
        glm::vec3 position    = {0.0f, 0.0f, 0.0f};
        glm::vec3 color       = {0.0f, 0.0f, 0.0f};
        glm::vec3 intensity   = {0.0f, 0.0f, 0.0f};
        glm::vec3 attenuation = {0.0f, 0.0f, 0.0f};
        glm::vec3 direction   = {0.0f, 0.0f, 0.0f};
        glm::vec2 cutOff      = {0.0f, 0.0f};
    };
}

#endif