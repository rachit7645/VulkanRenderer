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

#ifndef AABB_GLSL
#define AABB_GLSL

#include "GLSL.h"

GLSL_NAMESPACE_BEGIN(GPU)

struct AABB
{
    GLSL_VEC3 min;
    GLSL_VEC3 max;
};

#ifndef __cplusplus

#include "Constants.glsl"

vec3[8] GetAABBCorners(AABB aabb)
{
    const vec3 corners[8] =
    {
        vec3(aabb.min.x, aabb.min.y, aabb.min.z),
        vec3(aabb.min.x, aabb.min.y, aabb.max.z),
        vec3(aabb.min.x, aabb.max.y, aabb.min.z),
        vec3(aabb.min.x, aabb.max.y, aabb.max.z),
        vec3(aabb.max.x, aabb.min.y, aabb.min.z),
        vec3(aabb.max.x, aabb.min.y, aabb.max.z),
        vec3(aabb.max.x, aabb.max.y, aabb.min.z),
        vec3(aabb.max.x, aabb.max.y, aabb.max.z)
    };

    return corners;
}

AABB TransformAABB(AABB aabb, mat4 transform)
{
    vec3[8] corners = GetAABBCorners(aabb);

    AABB newAABB;

    newAABB.min = vec3( FLOAT_MAX);
    newAABB.max = vec3(-FLOAT_MAX);

    for (uint i = 0; i < 8; ++i)
    {
        vec4 transformedCorner = transform * vec4(corners[i], 1.0f);

        newAABB.min = min(newAABB.min, transformedCorner.xyz);
        newAABB.max = max(newAABB.max, transformedCorner.xyz);
    }

    return newAABB;
}

#endif

GLSL_NAMESPACE_END

#endif