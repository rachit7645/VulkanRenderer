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

struct AABB
{
    vec3 AABBmin;
    vec3 AABBmax;
};

vec3[8] GetAABBCorners(AABB aabb)
{
    const vec3 corners[8] =
    {
        vec3(aabb.AABBmin.x, aabb.AABBmin.y, aabb.AABBmin.z),
        vec3(aabb.AABBmin.x, aabb.AABBmin.y, aabb.AABBmax.z),
        vec3(aabb.AABBmin.x, aabb.AABBmax.y, aabb.AABBmin.z),
        vec3(aabb.AABBmin.x, aabb.AABBmax.y, aabb.AABBmax.z),
        vec3(aabb.AABBmax.x, aabb.AABBmin.y, aabb.AABBmin.z),
        vec3(aabb.AABBmax.x, aabb.AABBmin.y, aabb.AABBmax.z),
        vec3(aabb.AABBmax.x, aabb.AABBmax.y, aabb.AABBmin.z),
        vec3(aabb.AABBmax.x, aabb.AABBmax.y, aabb.AABBmax.z)
    };

    return corners;
}

AABB TransformAABB(AABB aabb, mat4 transform)
{
    vec3[8] corners = GetAABBCorners(aabb);

    AABB newAABB;

    vec4 firstCorner = transform * vec4(corners[0], 1.0);

    newAABB.AABBmin = firstCorner.xyz;
    newAABB.AABBmax = firstCorner.xyz;

    for (uint i = 1; i < 8; ++i)
    {
        vec4 transformedCorner = transform * vec4(corners[i], 1.0f);

        newAABB.AABBmin = min(newAABB.AABBmin, transformedCorner.xyz);
        newAABB.AABBmax = max(newAABB.AABBmax, transformedCorner.xyz);
    }

    return newAABB;
}

#endif