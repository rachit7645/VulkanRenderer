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

#ifndef SPOT_SHADOW_MAP_GLSL
#define SPOT_SHADOW_MAP_GLSL

#include "Math.glsl"
#include "Constants.glsl"
#include "Lights.glsl"

float CalculateSpotShadow
(
    uint lightIndex,
    ShadowedSpotLight light,
    vec3 fragPosition,
    vec3 normal,
    texture2DArray spotShadowMap,
    sampler shadowSampler
)
{
    vec4 lightSpacePos = light.matrix * vec4(fragPosition, 1.0f);

    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
         projCoords = projCoords * 0.5f + 0.5f;

    float currentDepth = projCoords.z;

    if (currentDepth < 0.0f || currentDepth > 1.0f)
    {
        return 0.0f;
    }

    // Calculate slope-scaled bias
    float cosTheta = clamp(dot(normal, normalize(light.position - fragPosition)), 0.0f, 1.0f);
    float bias     = MIN_SPOT_SHADOW_BIAS * TanArcCos(cosTheta);
    bias           = clamp(bias, 0.0f, MAX_SPOT_SHADOW_BIAS);

    return texture(sampler2DArrayShadow(spotShadowMap, shadowSampler), vec4(projCoords.xy, lightIndex, currentDepth - bias));
}

#endif