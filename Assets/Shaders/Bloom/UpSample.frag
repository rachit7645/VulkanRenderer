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

#include "MegaSet.glsl"
#include "Bloom/UpSample.h"

layout(location = 0) in vec2 fragUV;

layout (location = 0) out vec3 upsample;

vec3 SampleSource(float x, float y);

void main()
{
    // Radius is in texture coordinate space so that it varies per mip level
    float x = Constants.FilterRadius;
    float y = Constants.FilterRadius;

    // A - B - C
    vec3 a = SampleSource(-x,    y);
    vec3 b = SampleSource( 0.0f, y);
    vec3 c = SampleSource( x,    y);

    // D - E - F
    vec3 d = SampleSource(-x,    0.0f);
    vec3 e = SampleSource( 0.0f, 0.0f);
    vec3 f = SampleSource( x,    0.0f);

    // G - H - I
    vec3 g = SampleSource(-x,    -y);
    vec3 h = SampleSource( 0.0f, -y);
    vec3 i = SampleSource( x,    -y);

    // Apply weighted distribution using a 3x3 tent filter
    upsample  = e * 4.0f;
    upsample += (b + d + f + h) * 2.0f;
    upsample += (a + c + g + i);
    upsample *= 1.0f / 16.0f;
}

vec3 SampleSource(float x, float y)
{
    return texture(sampler2D(Textures[Constants.ImageIndex], Samplers[Constants.SamplerIndex]), fragUV + vec2(x, y)).rgb;
}