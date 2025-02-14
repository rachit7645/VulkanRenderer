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

float CalculateBias(uint layer, vec3 lightDir, vec3 normal, CascadeBuffer cascadeBuffer)
{
    // Slope-scaled bias
    float cosTheta = clamp(dot(normal, lightDir), 0.0f, 1.0f);
    float bias     = SHADOW_MIN_BIAS * TanArcCos(cosTheta);
    bias           = clamp(bias, 0.0f, SHADOW_MAX_BIAS);
    bias          *= 1.0f / (cascadeBuffer.cascades[layer].cascadeDistance * SHADOW_BIAS_MODIFIER);

    return bias;
}

float SampleShadow
(
    uint layer,
    vec2 texelSize,
    vec3 fragPosition,
    vec3 viewPosition,
    vec3 normal,
    vec3 lightDir,
    texture2DArray shadowMap,
    sampler shadowSampler,
    CascadeBuffer cascadeBuffer
)
{
    vec4 lightSpacePos = cascadeBuffer.cascades[layer].matrix * vec4(fragPosition, 1.0f);

    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords      = projCoords * 0.5f + 0.5f;

    float currentDepth = projCoords.z;

    float shadow = 1.0f;

    if (currentDepth < -1.0f || currentDepth > 1.0f)
    {
        return shadow;
    }

    float bias = CalculateBias(layer, lightDir, normal, cascadeBuffer);
    shadow     = 0.0f;

    #if CSM_ENABLE_PCF == 1

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

    #else

    shadow = texture(sampler2DArrayShadow(shadowMap, shadowSampler), vec4(projCoords.xy, layer, currentDepth - bias));

    #endif

    return shadow;
}

#if CSM_ENABLE_SMOOTH_TRANSITION == 1

float GetShadowBlendFactor(float viewDepth, float cascadeFar)
{
    return clamp(smoothstep(cascadeFar - SHADOW_BLEND_RANGE, cascadeFar, viewDepth), 0.0f, 1.0f);
}

void GetCurrentLayers
(
    uint lightIndex,
    int cascadeCount,
    vec3 viewPosition,
    CascadeBuffer cascadeBuffer,
    out uint layerA,
    out uint layerB,
    out float blendFactor
)
{
    for (int i = 0; i < cascadeCount - 1; ++i)
    {
        float nearDist = cascadeBuffer.cascades[lightIndex * cascadeCount + i    ].cascadeDistance;
        float farDist  = cascadeBuffer.cascades[lightIndex * cascadeCount + i + 1].cascadeDistance;

        if (viewPosition.z > farDist)
        {
            layerA      = i;
            layerB      = i + 1;
            blendFactor = GetShadowBlendFactor(viewPosition.z, farDist);

            return;
        }
    }

    // Last cascade contains the whole scene
    layerA      = cascadeCount - 1;
    layerB      = cascadeCount - 1;
    blendFactor = 0.0f;
}

float CalculateShadow
(
    uint lightIndex,
    vec3 fragPosition,
    vec3 viewPosition,
    vec3 normal,
    vec3 lightDir,
    CascadeBuffer cascadeBuffer,
    texture2DArray shadowMap,
    sampler shadowSampler
)
{
    ivec3 shadowMapSize = textureSize(sampler2DArrayShadow(shadowMap, shadowSampler), 0);

    uint  layerA;
    uint  layerB;
    float blendFactor;

    GetCurrentLayers
    (
        lightIndex,
        shadowMapSize.z,
        viewPosition,
        cascadeBuffer,
        layerA,
        layerB,
        blendFactor
    );

    vec2 texelSize = 1.0f / vec2(shadowMapSize.xy);

    float shadowA = SampleShadow
    (
        layerA,
        texelSize,
        fragPosition,
        viewPosition,
        normal,
        lightDir,
        shadowMap,
        shadowSampler,
        cascadeBuffer
    );

    float shadowB = SampleShadow
    (
        layerB,
        texelSize,
        fragPosition,
        viewPosition,
        normal,
        lightDir,
        shadowMap,
        shadowSampler,
        cascadeBuffer
    );

    return mix(shadowA, shadowB, blendFactor);
}

#else

uint GetCurrentLayer(uint lightIndex, int cascadeCount, vec3 viewPosition, CascadeBuffer cascadeBuffer)
{
    for (int i = 0; i < cascadeCount; ++i)
    {
        if (viewPosition.z > cascadeBuffer.cascades[lightIndex * cascadeCount + i].cascadeDistance)
        {
            return i;
        }
    }

    // Last one contains the entire scene
    return lightIndex * cascadeCount + cascadeCount - 1;
}

float CalculateShadow
(
    uint lightIndex,
    vec3 fragPosition,
    vec3 viewPosition,
    vec3 normal,
    vec3 lightDir,
    CascadeBuffer cascadeBuffer,
    texture2DArray shadowMap,
    sampler shadowSampler
)
{
    ivec3 shadowMapSize = textureSize(sampler2DArrayShadow(shadowMap, shadowSampler), 0);
    vec2  texelSize     = 1.0f / vec2(shadowMapSize.xy);
    uint  layer         = GetCurrentLayer(lightIndex, shadowMapSize.z, viewPosition, cascadeBuffer);

    return SampleShadow
    (
        layer,
        texelSize,
        fragPosition,
        viewPosition,
        normal,
        lightDir,
        shadowMap,
        shadowSampler,
        cascadeBuffer
    );
}

#endif

#endif