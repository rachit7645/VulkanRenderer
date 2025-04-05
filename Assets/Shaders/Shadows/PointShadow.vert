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

#include "Constants/Shadows/PointShadow.glsl"

layout(location = 0) out vec3 fragPosition;

// Note: Writing the shader in this compact way reduces register pressure a lot (for some reason)
void main()
{
    vec4 fragPos = Constants.Meshes.meshes[Constants.VisibleMeshes.indices[gl_DrawID]].transform * vec4(Constants.Positions.positions[gl_VertexIndex], 1.0f);
    fragPosition = fragPos.xyz;
    gl_Position  = Constants.Scene.shadowedPointLights.lights[Constants.LightIndex].matrices[Constants.FaceIndex] * fragPos;
}