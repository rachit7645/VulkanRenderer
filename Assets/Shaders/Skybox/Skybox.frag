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

#include "MegaSet.glsl"
#include "Skybox/Skybox.h"

layout(location = 0) in noperspective vec3 fragCurrentPosition;
layout(location = 1) in noperspective vec3 fragPreviousPosition;
layout(location = 2) in               vec3 fragUV;

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec2 gMotionVectors;

void main()
{
    outColor = texture(samplerCube(Cubemaps[Constants.CubemapIndex], Samplers[Constants.SamplerIndex]), fragUV).rgb;

    vec2 currentUV  = (fragCurrentPosition.xy  / fragCurrentPosition.z ) * 0.5f + 0.5f;
    vec2 previousUV = (fragPreviousPosition.xy / fragPreviousPosition.z) * 0.5f + 0.5f;

    gMotionVectors = currentUV - previousUV;
}