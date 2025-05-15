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

#ifndef MESH_GLSL
#define MESH_GLSL

#include "GLSL.h"
#include "Material.h"
#include "AABB.h"

GLSL_NAMESPACE_BEGIN(GPU)

struct Mesh
{
    mat4     transform;
    mat3     normalMatrix;
    Material material;
    AABB     aabb;
};

#ifndef __cplusplus

layout(buffer_reference, scalar) readonly buffer MeshBuffer
{
    Mesh meshes[];
};

#endif

GLSL_NAMESPACE_END

#endif