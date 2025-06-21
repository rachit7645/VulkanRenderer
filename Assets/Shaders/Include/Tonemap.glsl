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

// ACES Tonemap Operator

#ifndef ACES_GLSL
#define ACES_GLSL

#include "Math.glsl"

// https://github.com/dmnsgn/glsl-tone-map/blob/main/aces.glsl
vec3 ACESFast(vec3 color)
{
    return saturate((color * (2.51f * color + 0.03f)) / (color * (2.43f * color + 0.59f) + 0.14f));
}

// Stephen Hill (@self_shadow)
vec3 RRTAndODTFit(vec3 v)
{
    vec3 a = v * (v + 0.0245786f) - 0.000090537f;
    vec3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;

    return a / b;
}

vec3 ACESFitted(vec3 color)
{
    // sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
    const mat3 ACESInputMat = mat3
    (
        0.59719f, 0.35458f, 0.04823f,
        0.07600f, 0.90834f, 0.01566f,
        0.02840f, 0.13383f, 0.83777f
    );

    // ODT_SAT => XYZ => D60_2_D65 => sRGB
    const mat3 ACESOutputMat = mat3
    (
         1.60475f, -0.53108f, -0.07367f,
        -0.10208f,  1.10813f, -0.00605f,
        -0.00327f, -0.07276f,  1.07602f
    );

    color = color * ACESInputMat;

    // Apply RRT and ODT
    color = RRTAndODTFit(color);

    color = color * ACESOutputMat;

    // Clamp to [0, 1]
    color = saturate(color);

    return color;
}

// https://github.com/dmnsgn/glsl-tone-map/blob/main/uchimura.glsl
vec3 Uchimura(vec3 x, float P, float a, float m, float l, float c, float b)
{
    float l0 = ((P - m) * l) / a;
    float L0 = m - m / a;
    float L1 = m + (1.0f - m) / a;
    float S0 = m + l0;
    float S1 = m + a * l0;
    float C2 = (a * P) / (P - S1);
    float CP = -C2 / P;

    vec3 w0 = vec3(1.0f - smoothstep(0.0f, m, x));
    vec3 w2 = vec3(step(m + l0, x));
    vec3 w1 = vec3(1.0f - w0 - w2);

    vec3 T = vec3(m * pow(x / m, vec3(c)) + b);
    vec3 S = vec3(P - (P - S1) * exp(CP * (x - S0)));
    vec3 L = vec3(m + a * (x - m));

    return T * w0 + L * w1 + S * w2;
}

vec3 Uchimura(vec3 x)
{
    const float P = 1.0f;  // Max display brightness
    const float a = 1.0f;  // Contrast
    const float m = 0.22f; // Linear section start
    const float l = 0.4f;  // Linear section length
    const float c = 1.33f; // Black
    const float b = 0.0f;  // Pedestal

    return Uchimura(x, P, a, m, l, c, b);
}

#endif