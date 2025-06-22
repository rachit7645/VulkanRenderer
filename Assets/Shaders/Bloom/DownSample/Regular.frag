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
#extension GL_EXT_scalar_block_layout  : enable

#include "Constants.glsl"
#include "MegaSet.glsl"
#include "Bloom/DownSample.h"

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec3 downsample;

vec3 SampleOffset(float x, float y);

void main()
{
    vec2 srcTexelSize = 1.0f / vec2(textureSize(sampler2D(Textures[Constants.ImageIndex], Samplers[Constants.SamplerIndex]), 0));
    
    float x = srcTexelSize.x;
    float y = srcTexelSize.y;

    // A - B - C
    vec3 a = SampleOffset(-2.0f * x, 2.0f * y);
    vec3 b = SampleOffset( 0.0f,     2.0f * y);
    vec3 c = SampleOffset( 2.0f * x, 2.0f * y);

    // D - E - F
    vec3 d = SampleOffset(-2.0f * x, 0.0f);
    vec3 e = SampleOffset( 0.0f,     0.0f);
    vec3 f = SampleOffset( 2.0f * x, 0.0f);

    // G - H - I
    vec3 g = SampleOffset(-2.0f * x, -2.0f * y);
    vec3 h = SampleOffset( 0.0f,     -2.0f * y);
    vec3 i = SampleOffset( 2.0f * x, -2.0f * y);

    // - J - K -
    vec3 k = SampleOffset( x, y);
    vec3 j = SampleOffset(-x, y);

    // - L - M -
    vec3 l = SampleOffset(-x, -y);
    vec3 m = SampleOffset( x, -y);

    // Apply weighted distribution
    downsample  = e * 0.125f;
    downsample += (a + c + g + i) * 0.03125f;
    downsample += (b + d + f + h) * 0.0625f;
    downsample += (j + k + l + m) * 0.125f;

    // Fix for black spots
    downsample = max(downsample, 0.0001f);
}

vec3 SampleOffset(float x, float y)
{
    return texture(sampler2D(Textures[Constants.ImageIndex], Samplers[Constants.SamplerIndex]), fragUV + vec2(x, y)).rgb;
}