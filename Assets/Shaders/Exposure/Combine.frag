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

#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_scalar_block_layout  : enable

#include "MegaSet.glsl"
#include "Exposure/Combine.h"

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec3 outColor;

void main()
{
    vec3  hdrColor = texture(sampler2D(Textures[Constants.SceneColorIndex], Samplers[Constants.PointSamplerIndex]), fragUV).rgb;
    float exposure = texture(sampler2D(Textures[Constants.ExposureIndex],   Samplers[Constants.PointSamplerIndex]), vec2(0.0f)).r;

    outColor = hdrColor * pow(2.0f, exposure);
}