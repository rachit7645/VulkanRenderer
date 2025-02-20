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
#extension GL_EXT_multiview            : enable

#include "Constants/PointShadow.glsl"

layout(location = 0) out      vec3  fragPosition;
layout(location = 1) out flat vec3  fragLightPosition;
layout(location = 2) out flat float fragShadowFarPlane;

void main()
{
    Mesh            mesh       = Constants.Meshes.meshes[gl_DrawID];
    vec3            position   = Constants.Positions.positions[gl_VertexIndex];
    PointLight      light      = Constants.Scene.pointLights.lights[Constants.LightIndex];
    PointShadowData shadowData = Constants.PointShadows.pointShadowData[Constants.LightIndex];

    vec4 fragPos = mesh.transform * vec4(position, 1.0f);
    fragPosition = fragPos.xyz;
    gl_Position  = shadowData.matrices[gl_ViewIndex] * fragPos;

    fragLightPosition  = light.position;
    fragShadowFarPlane = shadowData.shadowPlanes.y;
}