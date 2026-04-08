/*
 * Copyright (c) 2023 - 2026 Rachit
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

#ifndef ENCODING_GLSL
#define ENCODING_GLSL

#include "Math.glsl"

// https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding
// https://twitter.com/Stubbesaurus/status/937994790553227264

vec2 OctahedronWrap(vec2 vector)
{
    return (1.0f - abs(vector.yx)) * vec2(vector.x >= 0.0f ? 1.0f : -1.0f, vector.y >= 0.0f ? 1.0f : -1.0f);
}

vec2 PackNormal(vec3 normal)
{
    normal /= abs(normal.x) + abs(normal.y) + abs(normal.z);

    normal.xy = normal.z >= 0.0f ? normal.xy : OctahedronWrap(normal.xy);
    normal.xy = normal.xy * 0.5f + 0.5f;

    return normal.xy;
}

vec3 UnpackNormal(vec2 pNormal)
{
    pNormal = pNormal * 2.0f - 1.0f;

    vec3 normal = vec3(pNormal.x, pNormal.y, 1.0f - abs(pNormal.x) - abs(pNormal.y));

    float flag = max(-normal.z, 0.0f);

    normal.x += normal.x >= 0.0f ? -flag : flag;
    normal.y += normal.y >= 0.0f ? -flag : flag;

    return normalize(normal);
}

float PackIoR(float ior)
{
    // Assume that IoR is between 1.0 and 3.0
    ior = clamp(ior, 1.0f, 3.0f);

    return 0.5f * ior - 0.5f;
}

float UnpackIoR(float ior)
{
    return 2.0f * ior + 1.0f;
}

#endif