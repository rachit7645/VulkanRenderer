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

#include "GPU/Lights.h"

float CalculatePointShadow
(
    uint lightIndex,
    ShadowedPointLight light,
    vec3 fragPosition,
    textureCubeArray pointShadowMap,
    sampler pointShadowSampler
)
{
    vec3  fragToLight     = fragPosition - light.position;
    float currentDistance = length(fragToLight);

    float shadow = texture
    (
        samplerCubeArrayShadow(pointShadowMap, pointShadowSampler),
        vec4(fragToLight, lightIndex),
        currentDistance - POINT_SHADOW_BIAS
    );

    return shadow;
}

#endif