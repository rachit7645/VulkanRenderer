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

#include "Constants/SSAOBlur.glsl"
#include "MegaSet.glsl"

layout(location = 0) in vec2 fragUV;

layout(location = 0) out float outSSAO;

void main()
{
    vec2 texelSize = 1.0f / vec2(textureSize(sampler2D(Textures[Constants.ImageIndex], Samplers[Constants.SamplerIndex]), 0));

    float blur = 0.0f;

    for (int x = -2; x < 2; ++x)
    {
        for (int y = -2; y < 2; ++y)
        {
            vec2 offset = vec2(x, y) * texelSize;
            blur       += texture(sampler2D(Textures[Constants.ImageIndex], Samplers[Constants.SamplerIndex]), fragUV + offset).r;
        }
    }

    outSSAO = clamp(blur / (4.0f * 4.0f), 0.0f, 1.0f);
}  

