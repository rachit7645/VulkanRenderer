//    Copyright 2023 - 2024 Rachit Khandelwal
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.

#ifndef FORWARD_SCENE_BUFFER_H
#define FORWARD_SCENE_BUFFER_H

#include <vulkan/vulkan.h>

#include "Externals/GLM.h"
#include "Vulkan/Util.h"
#include "Renderer/DirLight.h"

namespace Renderer::Forward
{
    struct VULKAN_GLSL_DATA SceneBuffer
    {
        glm::mat4 projection = {};
        glm::mat4 view       = {};
        glm::vec4 cameraPos  = {};
        DirLight  dirLight   = {};
    };
}

#endif
