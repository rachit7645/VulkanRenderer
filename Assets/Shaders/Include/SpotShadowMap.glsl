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

#include "GPU/Lights.h"
#include "Math.glsl"

float CalculateSpotShadow
(
    uint lightIndex,
    ShadowedSpotLight light,
    vec3 fragPosition,
    vec3 normal,
    texture2DArray spotShadowMap,
    sampler spotShadowSampler
)
{
    vec4 lightSpacePosition = light.matrix * vec4(fragPosition, 1.0f);

    vec3 projCoords    = lightSpacePosition.xyz / lightSpacePosition.w;
         projCoords.xy = projCoords.xy * 0.5f + 0.5f;

    vec3 lightDirection = normalize(light.position - fragPosition);

    float cosTheta = saturate(dot(normal, lightDirection));
    float bias     = SPOT_SHADOW_MIN_BIAS * FastTanArcCos(cosTheta);
    bias           = clamp(bias, 0.0f, SPOT_SHADOW_MAX_BIAS);

    float currentDepth = projCoords.z;

    float shadow = texture
    (
        sampler2DArrayShadow(spotShadowMap, spotShadowSampler),
        vec4(projCoords.xy, lightIndex, currentDepth + bias)
    );

    return shadow;
}

#endif