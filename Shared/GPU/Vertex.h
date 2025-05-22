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

#ifndef VERTEX_GLSL
#define VERTEX_GLSL

#include "GLSL.h"

GLSL_NAMESPACE_BEGIN(GPU)

struct Vertex
{
    GLSL_VEC3 normal;
    GLSL_VEC2 uv0;
    GLSL_VEC4 tangent;
};

#ifndef __cplusplus

layout(buffer_reference, scalar) readonly buffer VertexBuffer
{
    Vertex vertices[];
};

layout(buffer_reference, scalar) readonly buffer PositionBuffer
{
    vec3 positions[];
};

layout(buffer_reference, scalar) readonly buffer IndexBuffer
{
    uint indices[];
};

#endif

#ifdef __cplusplus

using Position = glm::vec3;
using Index    = u32;

template<typename T>
concept IsVertexType = std::is_same_v<T, Index   > ||
                       std::is_same_v<T, Position> ||
                       std::is_same_v<T, Vertex  >  ;

#endif

GLSL_NAMESPACE_END

#endif