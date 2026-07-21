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

vec2 PackOctahedron(vec3 vector)
{
    vector /= abs(vector.x) + abs(vector.y) + abs(vector.z);

    vector.xy = vector.z >= 0.0f ? vector.xy : OctahedronWrap(vector.xy);

    return vector.xy;
}

vec3 UnpackOctahedron(vec2 packedVector)
{
    vec3 vector = vec3(packedVector.x, packedVector.y, 1.0f - abs(packedVector.x) - abs(packedVector.y));

    float flag = max(-vector.z, 0.0f);

    vector.x += vector.x >= 0.0f ? -flag : flag;
    vector.y += vector.y >= 0.0f ? -flag : flag;

    return normalize(vector);
}

vec2 PackNormalFromMapToGBuffer(vec3 normal)
{
    normal.xy = PackOctahedron(normal) * 0.5f + 0.5f;

    return normal.xy;
}

vec3 UnpackNormalFromGBuffer(vec2 pNormal)
{
    pNormal = pNormal * 2.0f - 1.0f;

    return UnpackOctahedron(pNormal);
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