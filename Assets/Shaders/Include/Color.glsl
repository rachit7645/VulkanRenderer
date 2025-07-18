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
#include "Math.glsl"

vec3 SRGBToLinear(vec3 srgb)
{
    return vec3
    (
        srgb.r <= 0.04045f ? srgb.r / 12.92f : pow((srgb.r + 0.055f) / 1.055f, 2.4f),
        srgb.g <= 0.04045f ? srgb.g / 12.92f : pow((srgb.g + 0.055f) / 1.055f, 2.4f),
        srgb.b <= 0.04045f ? srgb.b / 12.92f : pow((srgb.b + 0.055f) / 1.055f, 2.4f)
    );
}

vec3 LinearToSRGB(vec3 linear)
{
    return vec3
    (
        linear.r <= 0.0031308f ? linear.r * 12.92f : 1.055f * pow(linear.r, 1.0f / 2.4f) - 0.055f,
        linear.g <= 0.0031308f ? linear.g * 12.92f : 1.055f * pow(linear.g, 1.0f / 2.4f) - 0.055f,
        linear.b <= 0.0031308f ? linear.b * 12.92f : 1.055f * pow(linear.b, 1.0f / 2.4f) - 0.055f
    );
}

float ToLuminance(vec3 color)
{
    return dot(color, vec3(0.2126f, 0.7152f, 0.0722f));
}

float KarisAverage(vec3 color)
{
    float luma = 0.25f * ToLuminance(color);

    return 1.0f / (1.0f + luma);
}

// https://github.com/playdeadgames/temporal/blob/master/Assets/Shaders/TemporalReprojection.shader

vec3 RGBToYCoCg(vec3 color)
{
    float y  = ( color.r / 4.0f) + (color.g / 2.0f) + (color.b / 4.0f);
    float co = ( color.r / 2.0f) - (color.b / 2.0f);
    float cg = (-color.r / 4.0f) + (color.g / 2.0f) - (color.b / 4.0f);

    return vec3(y, co, cg);
}

vec3 YCoCgToRGB(vec3 color)
{
    float r = color.x + color.y - color.z;
    float g = color.x + color.z;
    float b = color.x - color.y - color.z;

    return saturate(vec3(r, g, b));
}

#endif