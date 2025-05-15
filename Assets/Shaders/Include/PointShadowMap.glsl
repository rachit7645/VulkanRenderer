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

#include "Lights.h"

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
    const uint POINT_SHADOW_NUM_SAMPLES = 16;

    const vec3 POISSON_DISK[POINT_SHADOW_NUM_SAMPLES] = vec3[]
    (
        vec3(-0.94201624f, -0.39906216f,  0.1798099f),
        vec3( 0.94558609f, -0.76890725f, -0.3212261f),
        vec3(-0.09418411f,  0.92938870f,  0.2608733f),
        vec3( 0.34495938f,  0.29387760f, -0.8795000f),
        vec3(-0.91588581f,  0.45771432f, -0.5430732f),
        vec3(-0.81544232f, -0.87912464f, -0.4404469f),
        vec3(-0.38277543f,  0.27676845f,  0.7780390f),
        vec3( 0.97484398f,  0.75648379f,  0.4144236f),
        vec3( 0.44323325f, -0.97511554f,  0.5330471f),
        vec3( 0.53742981f, -0.47373420f, -0.7964774f),
        vec3(-0.26496911f, -0.41893023f,  0.7937408f),
        vec3( 0.79197514f,  0.19090188f, -0.2344887f),
        vec3(-0.24188840f,  0.99706507f, -0.5591046f),
        vec3(-0.81409955f,  0.91437590f,  0.6983244f),
        vec3( 0.19984126f,  0.78641367f,  0.3863599f),
        vec3( 0.14383161f, -0.14100790f,  0.9074430f)
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
            vec4(fragToLight + POISSON_DISK[i] * diskRadius, lightIndex),
            (currentDepth - POINT_SHADOW_BIAS) / pointLightShadowPlanes.y
        );
    }

    shadow /= float(POINT_SHADOW_NUM_SAMPLES);

    return shadow;
}

#endif