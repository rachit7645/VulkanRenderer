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

#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference2    : enable
#extension GL_EXT_scalar_block_layout  : enable

#include "Constants.glsl"
#include "MegaSet.glsl"
#include "Sampling.glsl"
#include "IBL/Convolution.h"

layout(location = 0) in vec3 worldPos;

layout(location = 0) out vec3 outColor;

void main()
{
    vec3 normal = normalize(worldPos);

    vec3 up    = abs(normal.z) < 0.999f ? vec3(0.0f, 0.0f, 1.0f) : vec3(1.0f, 0.0f, 0.0f);
    vec3 right = normalize(cross(up, normal));
    up         = normalize(cross(normal, right));

    vec2 resolution = vec2(textureSize(samplerCube(Cubemaps[Constants.EnvMapIndex], Samplers[Constants.SamplerIndex]), 0));

    float saTexel     = (4.0f * PI) / (6.0f * resolution.x * resolution.y);
    float sampleCount = float(IRRADIANCE_SAMPLE_COUNT);

    vec3 irradiance = vec3(0.0f);

    for (uint i = 0u; i < IRRADIANCE_SAMPLE_COUNT; ++i)
    {
        vec2 xi = Hammersley(i, IRRADIANCE_SAMPLE_COUNT);

        float phi      = TWO_PI * xi.x;
        float cosTheta = sqrt(xi.y);
        float sinTheta = sqrt(1.0f - xi.y);

        vec3 tangentSample = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
        vec3 sampleVec     = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;

        float pdf = cosTheta / PI;

        // Fix for extremely bright spots
        float saSample = 1.0f / max(sampleCount * pdf, 0.0001f);
        float mipLevel = max(0.5f * log2(saSample / saTexel), 0.0f);

        irradiance += textureLod(samplerCube(Cubemaps[Constants.EnvMapIndex], Samplers[Constants.SamplerIndex]), sampleVec, mipLevel).rgb;
    }

    // Irradiance = (π * I) / N
    // Diffuse = Lambert(Irradiance) * Albedo => [{(π * I) / N} / π] * Albedo => (I / N) * Albedo
    // OutColor = I / N => Diffuse = OutColor * Albedo
    // See PBR.glsl/CalculateAmbient(...)
    outColor = irradiance / sampleCount;
}