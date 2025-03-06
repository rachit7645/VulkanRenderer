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

#include "Constants/TAA.glsl"
#include "MegaSet.glsl"

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec3 resolvedOutput;

void main()
{
    vec2 velocity             = texture(sampler2D(Textures[Constants.VelocityIndex], Samplers[Constants.PointSamplerIndex]), fragUV).xy;
    vec2 prevousPixelPosition = fragUV - velocity;
    
    vec3 currentColor = texture(sampler2D(Textures[Constants.CurrentColorIndex],  Samplers[Constants.PointSamplerIndex]),  fragUV).rgb;
    vec3 historyColor = texture(sampler2D(Textures[Constants.HistoryBufferIndex], Samplers[Constants.LinearSamplerIndex]), fragUV).rgb;

    vec2 texelSize = 1.0f / vec2(textureSize(sampler2D(Textures[Constants.CurrentColorIndex],  Samplers[Constants.PointSamplerIndex]), 0));

    vec3 nearColor0 = texture(sampler2D(Textures[Constants.CurrentColorIndex], Samplers[Constants.PointSamplerIndex]), fragUV + texelSize * vec2( 1,  0)).rgb;
    vec3 nearColor1 = texture(sampler2D(Textures[Constants.CurrentColorIndex], Samplers[Constants.PointSamplerIndex]), fragUV + texelSize * vec2( 0,  1)).rgb;
    vec3 nearColor2 = texture(sampler2D(Textures[Constants.CurrentColorIndex], Samplers[Constants.PointSamplerIndex]), fragUV + texelSize * vec2(-1,  0)).rgb;
    vec3 nearColor3 = texture(sampler2D(Textures[Constants.CurrentColorIndex], Samplers[Constants.PointSamplerIndex]), fragUV + texelSize * vec2( 0, -1)).rgb;
    
    vec3 boxMin = min(currentColor, min(nearColor0, min(nearColor1, min(nearColor2, nearColor3))));
    vec3 boxMax = max(currentColor, max(nearColor0, max(nearColor1, max(nearColor2, nearColor3))));
    
    historyColor = clamp(historyColor, boxMin, boxMax);
    
    resolvedOutput = mix(currentColor, historyColor, Constants.ModulationFactor);
}