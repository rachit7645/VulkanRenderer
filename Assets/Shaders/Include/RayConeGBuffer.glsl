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

#ifndef RAY_CONE_GBUFFER_GLSL
#define RAY_CONE_GBUFFER_GLSL

// Texture Level of Detail Strategies for Real-Time Ray Tracing : Ray Tracing Gems, Chapter 20
float ComputeSurfaceSpreadAngle(vec3 position, vec3 normal)
{
    vec3 dPdx = dFdx(position);
    vec3 dPdy = dFdy(position);
    vec3 dNdx = dFdx(normal);
    vec3 dNdy = dFdy(normal);

    float phi = length(dNdx + dNdy);

    float s = sign(dot(dPdx, dNdx) + dot(dPdy, dNdy));

    // K1 = 1.0f, K2 = 0.0f
    float beta = 2.0f * s * phi;

    return beta;
}

#endif