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

#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference2    : enable
#extension GL_EXT_scalar_block_layout  : enable

#include "Constants.glsl"
#include "MegaSet.glsl"
#include "Sampling.glsl" 
#include "PBR.glsl"
#include "IBL/PreFilter.h"

layout(location = 0) in vec3 worldPos;

layout(location = 0) out vec3 outColor;

void main()
{
    vec3 N = normalize(worldPos);

    // Assume that the reflection, view and normal vector are the same
    vec3 R = N;
    vec3 V = R;

    // Tangent to World Space Sample Vector
    vec3 up = abs(N.z) < 0.999f ? vec3(0.0f, 0.0f, 1.0f) : vec3(1.0f, 0.0f, 0.0f);
    vec3 T  = normalize(cross(up, N));
    vec3 B  = cross(N, T);

    vec2 resolution = vec2(textureSize(samplerCube(Cubemaps[Constants.EnvMapIndex], Samplers[Constants.SamplerIndex]), 0));

    vec3  prefilteredColor = vec3(0.0f);
    float totalWeight      = 0.0f;

    float saTexel     = 4.0f * PI / (6.0f * resolution.x * resolution.y);
    float sampleCount = float(Constants.SampleCount);
    float r2          = Constants.Roughness * Constants.Roughness;

    for (uint i = 0u; i < Constants.SampleCount; ++i)
    {
        vec2 Xi = Hammersley(i, Constants.SampleCount);
        vec3 H  = ImportanceSampleGGX(Xi, N, T, B, Constants.Roughness);
        vec3 L  = normalize(2.0f * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0f);
        float NdotH = max(dot(N, H), 0.0f);
        float HdotV = max(dot(H, V), 0.0f);

        float D   = DistributionGGX(NdotH, r2);
        float pdf = D * NdotH / (4.0f * HdotV) + 0.0001f;

        // Fix for extremely bright spots
        float saSample = 1.0f / max(sampleCount * pdf, 0.0001f);
        float mipLevel = Constants.Roughness == 0.0f ? 0.0f : 0.5f * log2(saSample / saTexel);

        vec3 color = NdotL * textureLod(samplerCube(Cubemaps[Constants.EnvMapIndex], Samplers[Constants.SamplerIndex]), L, mipLevel).rgb;

        if (NdotL > 0.0f)
        {
            prefilteredColor += color;
            totalWeight      += NdotL;
        }
    }

    outColor = prefilteredColor / totalWeight;
}