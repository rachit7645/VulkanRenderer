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
#include "PBR.glsl"
#include "Sampling.glsl"

layout(location = 0) in vec2 fragUV;

layout (location = 0) out vec2 outColor;

void main()
{
    float NdotV     = fragUV.x;
    float roughness = fragUV.y;

    vec3 up = vec3(1.0f, 0.0f, 0.0f);
    vec3 N  = vec3(0.0f, 0.0f, 1.0f);

    // Tangent to World space
    vec3 tangent   = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);

    vec3 V = vec3
    (
        sqrt(1.0f - NdotV * NdotV),
        0.0f,
        NdotV
    );

    float A = 0.0f;
    float B = 0.0f;

    for (uint i = 0u; i < BRDF_LUT_SAMPLE_COUNT; ++i)
    {
        vec2 Xi = Hammersley(i, BRDF_LUT_SAMPLE_COUNT);
        vec3 H  = ImportanceSampleGGX(Xi, N, tangent, bitangent, roughness);
        vec3 L  = normalize(2.0f * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0f);
        float NdotH = max(H.z, 0.0f);
        float VdotH = max(dot(V, H), 0.0f);

        if (NdotL > 0.0f)
        {
            float G     = GeometrySmith_IBL(N, V, L, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc    = pow(1.0f - VdotH, 5.0f);

            A += (1.0f - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }

    A /= float(BRDF_LUT_SAMPLE_COUNT);
    B /= float(BRDF_LUT_SAMPLE_COUNT);

    outColor = vec2(A, B);
}