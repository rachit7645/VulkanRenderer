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

    // Tangent to World space
    const vec3 N = vec3(0.0f,  0.0f, 1.0f); // UP = (1, 0, 0)
    const vec3 T = vec3(0.0f, -1.0f, 0.0f); // normalize(UP x N)
    const vec3 B = vec3(1.0f,  0.0f, 0.0f); // cross(N, T)

    vec3 V = vec3(sqrt(1.0f - NdotV * NdotV), 0.0f, NdotV);

    float X = 0.0f;
    float Y = 0.0f;

    for (uint i = 0u; i < BRDF_LUT_SAMPLE_COUNT; ++i)
    {
        vec2 Xi = Hammersley(i, BRDF_LUT_SAMPLE_COUNT);
        vec3 H  = ImportanceSampleGGX(Xi, N, T, B, roughness);
        vec3 L  = normalize(2.0f * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0f);
        float NdotH = max(H.z, 0.0f);
        float VdotH = max(dot(V, H), 0.0f);

        if (NdotL > 0.0f)
        {
            float G     = GeometrySmith_IBL(N, V, L, roughness);
            float G_Vis = (G * VdotH) / max(NdotH * NdotV, 1e-7f);
            float Fc    = pow5(1.0f - VdotH);

            X += (1.0f - Fc) * G_Vis;
            Y += Fc * G_Vis;
        }
    }

    X /= float(BRDF_LUT_SAMPLE_COUNT);
    Y /= float(BRDF_LUT_SAMPLE_COUNT);

    outColor = vec2(X, Y);
}