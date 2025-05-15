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
#include "IBL/Convolution.h"

layout(location = 0) in vec3 worldPos;

layout(location = 0) out vec3 outColor;

void main()
{
    vec3 normal = normalize(worldPos);

    vec3 up    = vec3(0.0f, 1.0f, 0.0f);
    vec3 right = normalize(cross(up, normal));
    up         = normalize(cross(normal, right));

    vec3 irradiance  = vec3(0.0f);
    uint sampleCount = 0u;

    for (float phi = 0.0; phi < TWO_PI; phi += CONVOLUTION_SAMPLE_DELTA)
    {
        float sinPhi = sin(phi);
        float cosPhi = cos(phi);

        for (float theta = 0.0f; theta < HALF_PI; theta += CONVOLUTION_SAMPLE_DELTA)
        {
            float sinTheta = sin(theta);
            float cosTheta = cos(theta);

            vec3 tangentSample = vec3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);
            vec3 sampleVec     = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;

            irradiance += texture(samplerCube(Cubemaps[Constants.EnvMapIndex], Samplers[Constants.SamplerIndex]), sampleVec).rgb * cosTheta * sinTheta;

            ++sampleCount;
        }
    }

    outColor = PI * (irradiance / float(sampleCount));
}