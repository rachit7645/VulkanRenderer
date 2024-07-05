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

#ifndef SPOT_LIGHT_H
#define SPOT_LIGHT_H

#include "Externals/GLM.h"
#include "Util/Util.h"

namespace Renderer
{
    struct VULKAN_GLSL_DATA SpotLight
    {
        glm::vec4 position    = {0.0f, 0.0f, 0.0f, 1.0f};
        glm::vec4 color       = {0.0f, 0.0f, 0.0f, 1.0f};
        glm::vec4 intensity   = {0.0f, 0.0f, 0.0f, 1.0f};
        glm::vec4 attenuation = {0.0f, 0.0f, 0.0f, 1.0f};
        glm::vec4 direction   = {0.0f, 0.0f, 0.0f, 1.0f};
        glm::vec4 cutOff      = {0.0f, 0.0f, 1.0f, 1.0f};
    };
}

#endif