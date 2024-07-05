/*
 *   Copyright 2023 - 2024 Rachit Khandelwal
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#version 460

// Extensions
#extension GL_GOOGLE_include_directive : enable

// Includes
#include "GammaCorrect.glsl"
#include "ACES.glsl"

// Fragment inputs
layout(location = 0) in vec2 uv;

// Images
layout(set = 0, binding = 0) uniform sampler2D colorOutput;

// Fragment outputs
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(GammaCorrect(ACES(texture(colorOutput, uv).rgb)), 1.0f);
}