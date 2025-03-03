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
#include "Constants/UpSample.glsl"

layout(location = 0) in vec2 fragUV;

layout (location = 0) out vec3 upsample;

void main()
{
    #define SRC_TEXTURE sampler2D(Textures[Constants.ImageIndex], Samplers[Constants.SamplerIndex])
    
    // Radius is in texture coordinate space so that it varies per mip level
    float x = Constants.FilterRadius;
    float y = Constants.FilterRadius;

    // A - B - C
    vec3 a = texture(SRC_TEXTURE, vec2(fragUV.x - x, fragUV.y + y)).rgb;
    vec3 b = texture(SRC_TEXTURE, vec2(fragUV.x,     fragUV.y + y)).rgb;
    vec3 c = texture(SRC_TEXTURE, vec2(fragUV.x + x, fragUV.y + y)).rgb;

    // D - E - F
    vec3 d = texture(SRC_TEXTURE, vec2(fragUV.x - x, fragUV.y)).rgb;
    vec3 e = texture(SRC_TEXTURE, vec2(fragUV.x,     fragUV.y)).rgb;
    vec3 f = texture(SRC_TEXTURE, vec2(fragUV.x + x, fragUV.y)).rgb;

    // G - H - I
    vec3 g = texture(SRC_TEXTURE, vec2(fragUV.x - x, fragUV.y - y)).rgb;
    vec3 h = texture(SRC_TEXTURE, vec2(fragUV.x,     fragUV.y - y)).rgb;
    vec3 i = texture(SRC_TEXTURE, vec2(fragUV.x + x, fragUV.y - y)).rgb;

    // Apply weighted distribution using a 3x3 tent filter
    upsample  = e * 4.0f;
    upsample += (b + d + f + h) * 2.0f;
    upsample += (a + c + g + i);
    upsample *= 1.0f / 16.0f;
}