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
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout  : enable

#include "Constants.glsl"
#include "Color.glsl"
#include "MegaSet.glsl"
#include "Constants/DownSample.glsl"

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec3 downsample;

void main()
{
    #define SRC_TEXTURE sampler2D(Textures[Constants.ImageIndex], Samplers[Constants.SamplerIndex])

    vec2 srcTexelSize = 1.0f / vec2(textureSize(SRC_TEXTURE, 0));
    
    float x = srcTexelSize.x;
    float y = srcTexelSize.y;

    // A - B - C
    vec3 a = texture(SRC_TEXTURE, vec2(fragUV.x - 2.0f * x, fragUV.y + 2.0f * y)).rgb;
    vec3 b = texture(SRC_TEXTURE, vec2(fragUV.x,            fragUV.y + 2.0f * y)).rgb;
    vec3 c = texture(SRC_TEXTURE, vec2(fragUV.x + 2.0f * x, fragUV.y + 2.0f * y)).rgb;

    // D - E - F
    vec3 d = texture(SRC_TEXTURE, vec2(fragUV.x - 2.0f * x, fragUV.y)).rgb;
    vec3 e = texture(SRC_TEXTURE, vec2(fragUV.x,            fragUV.y)).rgb;
    vec3 f = texture(SRC_TEXTURE, vec2(fragUV.x + 2.0f * x, fragUV.y)).rgb;

    // G - H - I
    vec3 g = texture(SRC_TEXTURE, vec2(fragUV.x - 2.0f * x, fragUV.y - 2.0f * y)).rgb;
    vec3 h = texture(SRC_TEXTURE, vec2(fragUV.x,            fragUV.y - 2.0f * y)).rgb;
    vec3 i = texture(SRC_TEXTURE, vec2(fragUV.x + 2.0f * x, fragUV.y - 2.0f * y)).rgb;

    // - J - K -
    vec3 j = texture(SRC_TEXTURE, vec2(fragUV.x - x, fragUV.y + y)).rgb;
    vec3 k = texture(SRC_TEXTURE, vec2(fragUV.x + x, fragUV.y + y)).rgb;

    // - L - M -
    vec3 l = texture(SRC_TEXTURE, vec2(fragUV.x - x, fragUV.y - y)).rgb;
    vec3 m = texture(SRC_TEXTURE, vec2(fragUV.x + x, fragUV.y - y)).rgb;

    if (Constants.IsFirstSample == 1)
    {
        vec3 groups[5];
        
        groups[0] = (a + b + d + e) * (0.125f / 4.0f);
        groups[1] = (b + c + e + f) * (0.125f / 4.0f);
        groups[2] = (d + e + g + h) * (0.125f / 4.0f);
        groups[3] = (e + f + h + i) * (0.125f / 4.0f);
        groups[4] = (j + k + l + m) * (0.5f   / 4.0f);
        
        groups[0] *= KarisAverage(groups[0]);
        groups[1] *= KarisAverage(groups[1]);
        groups[2] *= KarisAverage(groups[2]);
        groups[3] *= KarisAverage(groups[3]);
        groups[4] *= KarisAverage(groups[4]);
        
        downsample = groups[0] + groups[1] + groups[2] + groups[3] + groups[4];
    }
    else
    {
        // Apply weighted distribution
        downsample  = e * 0.125f;
        downsample += (a + c + g + i) * 0.03125f;
        downsample += (b + d + f + h) * 0.0625f;
        downsample += (j + k + l + m) * 0.125f;
    }

    // Fix for black spots
    downsample = max(downsample, 0.0001f);
}