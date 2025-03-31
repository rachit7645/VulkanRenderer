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

#ifndef POINT_SHADOW_MAP_GLSL
#define POINT_SHADOW_MAP_GLSL

#include "Lights.glsl"

float CalculatePointShadow
(
    uint lightIndex,
    ShadowedPointLight light,
    vec2 pointLightShadowPlanes,
    vec3 fragPosition,
    vec3 cameraPosition,
    textureCubeArray pointShadowMap,
    sampler pointShadowSampler
)
{
    const vec3 GRID_SAMPLING_DISK[POINT_SHADOW_NUM_SAMPLES] = vec3[]
    (
        vec3(1.0f, 1.0f,  1.0f), vec3( 1.0f, -1.0f,  1.0f), vec3(-1.0f, -1.0f,  1.0f), vec3(-1.0f, 1.0f,  1.0f),
        vec3(1.0f, 1.0f, -1.0f), vec3( 1.0f, -1.0f, -1.0f), vec3(-1.0f, -1.0f, -1.0f), vec3(-1.0f, 1.0f, -1.0f),
        vec3(1.0f, 1.0f,  0.0f), vec3( 1.0f, -1.0f,  0.0f), vec3(-1.0f, -1.0f,  0.0f), vec3(-1.0f, 1.0f,  0.0f),
        vec3(1.0f, 0.0f,  1.0f), vec3(-1.0f,  0.0f,  1.0f), vec3( 1.0f,  0.0f, -1.0f), vec3(-1.0f, 0.0f, -1.0f),
        vec3(0.0f, 1.0f,  1.0f), vec3( 0.0f, -1.0f,  1.0f), vec3( 0.0f, -1.0f, -1.0f), vec3( 0.0f, 1.0f, -1.0f)
    );

    vec3  fragToLight  = fragPosition - light.position;
    float currentDepth = length(fragToLight);

    float shadow       = 0.0f;
    float viewDistance = length(cameraPosition - fragPosition);
    float diskRadius   = (1.0f + (viewDistance / pointLightShadowPlanes.y)) / pointLightShadowPlanes.y;

    for(uint i = 0; i < POINT_SHADOW_NUM_SAMPLES; ++i)
    {
        shadow += texture
        (
            samplerCubeArrayShadow(pointShadowMap, pointShadowSampler),
            vec4(fragToLight + GRID_SAMPLING_DISK[i] * diskRadius, lightIndex),
            (currentDepth - POINT_SHADOW_BIAS) / pointLightShadowPlanes.y
        );
    }

    shadow /= float(POINT_SHADOW_NUM_SAMPLES);

    return shadow;
}

#endif