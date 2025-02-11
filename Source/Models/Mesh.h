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

#ifndef MESH_H
#define MESH_H

#include "Info.h"
#include "Material.h"

namespace Models
{
    struct Mesh
    {
        Info      indexInfo    = {};
        Info      positionInfo = {};
        Info      vertexInfo   = {};
        Material  material     = {};
        glm::mat4 transform    = glm::identity<glm::mat4>();
    };
}

#endif
