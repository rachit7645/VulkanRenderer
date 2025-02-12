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

#ifndef CSM_GLSL
#define CSM_GLSL

#include "Math.glsl"
#include "Constants.glsl"

struct Cascade
{
    float cascadeDistance;
    mat4  matrix;
};

layout(buffer_reference, scalar) readonly buffer CascadeBuffer
{
    Cascade cascades[];
};

int GetCurrentLayer(int cascadeCount, vec3 viewPosition, CascadeBuffer cascadeBuffer)
{
    for (int i = 0; i < cascadeCount - 1; ++i)
    {
        if (viewPosition.z > cascadeBuffer.cascades[i].cascadeDistance)
        {
            return i;
        }
    }

    // Last one contains the entire scene
    return cascadeCount - 1;
}

float CalculateBias(int layer, vec3 lightDir, vec3 normal, CascadeBuffer cascadeBuffer)
{
    // Slope-scaled bias
    float cosTheta = clamp(dot(normal, lightDir), 0.0f, 1.0f);
    float bias     = SHADOW_MIN_BIAS * TanArcCos(cosTheta);
    bias           = clamp(bias, 0.0f, SHADOW_MAX_BIAS);
    bias          *= 1.0f / (cascadeBuffer.cascades[layer].cascadeDistance * SHADOW_BIAS_MODIFIER);

    return bias;
}

float CalculateShadow
(
    vec3 fragPosition,
    vec3 viewPosition,
    vec3 normal,
    vec3 lightDir,
    texture2DArray shadowMap,
    sampler shadowSampler,
    CascadeBuffer cascadeBuffer
)
{
    ivec3 shadowMapSize = textureSize(sampler2DArrayShadow(shadowMap, shadowSampler), 0);

    int layer = GetCurrentLayer(shadowMapSize.z, viewPosition, cascadeBuffer);

    vec4 lightSpacePos = cascadeBuffer.cascades[layer].matrix * vec4(fragPosition, 1.0f);

    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords      = projCoords * 0.5f + 0.5f;

    float currentDepth = projCoords.z;

    float shadow = 1.0f;

    if (currentDepth > -1.0f && currentDepth < 1.0f)
    {
        float bias      = CalculateBias(layer, lightDir, normal, cascadeBuffer);
        vec2  texelSize = 1.0f / vec2(shadowMapSize.xy);

        shadow           = 0.0f;
        uint sampleCount = 0;

        for (int x = -SHADOW_PCF_RANGE; x <= SHADOW_PCF_RANGE; x++)
        {
            for (int y = -SHADOW_PCF_RANGE; y <= SHADOW_PCF_RANGE; y++)
            {
                vec2 offset = vec2(x, y) * texelSize;
                shadow += texture(sampler2DArrayShadow(shadowMap, shadowSampler), vec4(projCoords.xy + offset, layer, currentDepth - bias));
                ++sampleCount;
            }
        }

        shadow /= sampleCount;
    }

    return shadow;
}

#endif