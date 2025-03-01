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

#ifndef SCENE_H
#define SCENE_H

#include "Objects/Lights.h"
#include "Externals/GLM.h"
#include "Vulkan/Util.h"

namespace Renderer
{
    struct Scene
    {
        glm::mat4       projection        = {};
        glm::mat4       inverseProjection = {};
        glm::mat4       view              = {};
        glm::mat4       inverseView       = {};
        glm::mat3       normalView        = {};
        glm::vec3       cameraPos         = {};
        VkDeviceAddress dirLights         = 0;
        VkDeviceAddress pointLights       = 0;
        VkDeviceAddress spotLights        = 0;
    };
}

#endif
