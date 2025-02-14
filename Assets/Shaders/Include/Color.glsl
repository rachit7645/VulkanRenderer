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

#ifndef COLOR_GLSL
#define COLOR_GLSL

#include "Constants.glsl"

vec3 GammaCorrect(vec3 color)
{
    return pow(color, vec3(GAMMA_FACTOR));
}

vec3 ToLinear(vec3 color)
{
    return pow(color, vec3(INV_GAMMA_FACTOR));
}

float ToLuminance(vec3 color)
{
    return dot(color, vec3(0.2126f, 0.7152f, 0.0722f));
}

float KarisAverage(vec3 color)
{
    float luma = ToLuminance(ToLinear(color)) * 0.25f;

    return 1.0f / (1.0f + luma);
}

#endif