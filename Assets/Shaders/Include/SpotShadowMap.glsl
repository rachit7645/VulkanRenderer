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

layout(buffer_reference, scalar) readonly buffer SpotShadowBuffer
{
    mat4 matrices[];
};

float CalculateSpotShadow
(
    uint lightIndex,
    vec3 fragPosition,
    vec3 normal,
    vec3 lightPosition,
    mat4 shadowMatrix,
    texture2DArray spotShadowMap,
    sampler shadowSampler
)
{
    vec4 lightSpacePos = shadowMatrix * vec4(fragPosition, 1.0f);

    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords      = projCoords * 0.5f + 0.5f;

    float currentDepth = projCoords.z;

    float shadow = 1.0f;

    if (currentDepth < -1.0f || currentDepth > 1.0f)
    {
        return shadow;
    }

    // Calculate slope-scaled bias
    float cosTheta = clamp(dot(normal, normalize(lightPosition)), 0.0f, 1.0f);
    float bias     = MIN_SPOT_SHADOW_BIAS * TanArcCos(cosTheta);
    bias           = clamp(bias, 0.0f, MAX_SPOT_SHADOW_BIAS);

    shadow = texture(sampler2DArrayShadow(spotShadowMap, shadowSampler), vec4(projCoords.xy, lightIndex, currentDepth - bias));

    return shadow;
}

#endif