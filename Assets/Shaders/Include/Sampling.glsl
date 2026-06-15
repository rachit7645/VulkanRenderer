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

#ifndef SAMPLING_GLSL
#define SAMPLING_GLSL

#include "Math.glsl"

float RadicalInverseVanDerCorput(uint bits)
{
    const float INVERSE_TWO_TO_THE_POWER_OF_32 = 2.3283064365386963e-10f;

    return INVERSE_TWO_TO_THE_POWER_OF_32 * bitfieldReverse(bits);
}

vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i) / float(N), RadicalInverseVanDerCorput(i));
}

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, vec3 T, vec3 B, float roughness)
{
    float a = roughness * roughness;

    float phi      = TWO_PI * Xi.x;
    float cosTheta = sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

    // Spherical to Cartesian halfway vector
    vec3 H = vec3
    (
        cos(phi) * sinTheta,
        sin(phi) * sinTheta,
        cosTheta
    );

    vec3 sampleVector = T * H.x + B * H.y + N * H.z;

    return normalize(sampleVector);
}

// Building an Orthonormal Basis, Revisited, Duff et al. 2017
void BuildOrthonormalBasis(vec3 N, out vec3 T, out vec3 B)
{
    float sign = (N.z >= 0.0f) ? 1.0f : -1.0f;

    float a = -1.0f / (sign + N.z);
    float b = N.x * N.y * a;

    T = vec3(1.0f + sign * N.x * N.x * a, sign * b, -sign * N.x);
    B = vec3(b, sign + N.y * N.y * a, -N.y);
}

// PBR Book
vec3 UniformSampleCone(vec2 point, float cosThetaMax)
{
    float cosTheta = (1.0f - point.x) + point.x * cosThetaMax;
    float sinTheta = sqrt(saturate(1.0f - cosTheta * cosTheta));

    float phi = TWO_PI * point.y;

    return vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

// Wish I had templates...
float HaltonBase2(uint index)
{
    return RadicalInverseVanDerCorput(index + 1);
}

float Halton(uint index, uint base)
{
    float result = 0.0f;
    float f      = 1.0f / float(base);

    index += 1;

    while (index > 0)
    {
        result += f * float(index % base);

        index /= base;
        f     /= float(base);
    }

    return result;
}

#endif