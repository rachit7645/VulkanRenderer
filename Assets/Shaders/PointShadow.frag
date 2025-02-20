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

layout(location = 0) in      vec3  fragPosition;
layout(location = 1) in flat vec3  fragLightPosition;
layout(location = 2) in flat float fragShadowFarPlane;

#include "Constants/PointShadow.glsl"

void main()
{
    float lightDistance = length(fragPosition - fragLightPosition);

    // Map to [0, 1] to store into depth buffer
    lightDistance = lightDistance / fragShadowFarPlane;

    gl_FragDepth = lightDistance;
}