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

layout(location = 0) in      vec3 fragPosition;
layout(location = 1) in flat int  fragViewIndex;

#include "Constants/PointShadow.glsl"

void main()
{
    PointLight      light           = Constants.Scene.pointLights.lights[Constants.LightIndex];
    PointShadowData pointShadowData = Constants.PointShadows.pointShadowData[Constants.LightIndex];

    float lightDistance = length(fragPosition - light.position);

    // Map to [0, 1]
    lightDistance = lightDistance / pointShadowData.shadowPlanes.y;

    gl_FragDepth = lightDistance;
}