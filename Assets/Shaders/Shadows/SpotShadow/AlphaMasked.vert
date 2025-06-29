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

#include "Shadows/SpotShadow/AlphaMasked.h"

layout(location = 0) out      vec2 fragUV;
layout(location = 1) out flat uint fragDrawID;

void main()
{
    uint     instanceIndex = Constants.InstanceIndices.indices[gl_DrawID];
    Instance instance      = Constants.Instances.instances[instanceIndex];
    Mesh     mesh          = Constants.Meshes.meshes[instance.meshIndex];

    vec3 position = Constants.Positions.positions[gl_VertexIndex];
    UV   uvs      = Constants.UVs.uvs[gl_VertexIndex];
    mat4 matrix   = Constants.Scene.ShadowedSpotLights.lights[Constants.LightIndex].matrix;

    vec4 fragPos = instance.transform * vec4(position, 1.0f);
    gl_Position  = matrix * fragPos;

    fragUV     = uvs.uv[mesh.material.albedoUVMapID];
    fragDrawID = instance.meshIndex;
}