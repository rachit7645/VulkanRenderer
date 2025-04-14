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

#ifndef CULLING_GLSL
#define CULLING_GLSL

#include "AABB.glsl"

struct Plane
{
    vec3  normal;
    float dist;
};

layout(buffer_reference, scalar) readonly buffer FrustumBuffer
{
    Plane planes[6];
};

layout(buffer_reference, scalar) buffer PreviouslyVisibleBuffer
{
    uint visibility[];
};

bool IsVisible(vec3[8] corners, FrustumBuffer frustum)
{
    for (uint i = 0; i < 6; ++i)
    {
        Plane plane = frustum.planes[i];

        uint outside = 0;

        for (uint j = 0; j < 8; ++j)
        {
            if ((dot(plane.normal, corners[j]) + plane.dist) < 0.0f)
            {
                ++outside;
            }
        }

        if (outside == 8)
        {
            return false;
        }
    }

    return true;
}

bool IsVisible(Mesh mesh, FrustumBuffer frustum)
{
    AABB    aabb    = TransformAABB(mesh.aabb, mesh.transform);
    vec3[8] corners = GetAABBCorners(aabb);

    return IsVisible(corners, frustum);
}

#endif