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

#include "GPU/Material.h"
#include "Deferred/GBuffer.h"

layout(location = 0) out VertexData
{
    vec2 uv[2];

    vec3 N;
    vec3 T;
    vec3 B;

    noperspective vec3 currentPosition;
    noperspective vec3 previousPosition;

    flat uint drawID;
} Output;

void main()
{
    uint     instanceIndex    = Constants.InstanceIndices.indices[gl_DrawID];
    Instance currentInstance  = Constants.CurrentInstances.instances[instanceIndex];
    Instance previousInstance = Constants.PreviousInstances.instances[instanceIndex];
    Mesh     currentMesh      = Constants.CurrentMeshes.meshes[currentInstance.meshIndex];
    Mesh     previousMesh     = Constants.PreviousMeshes.meshes[previousInstance.meshIndex];

    vec3   position = Constants.Positions.positions[gl_VertexIndex];
    UV     uvs      = Constants.UVs.uvs[gl_VertexIndex];
    Vertex vertex   = Constants.Vertices.vertices[gl_VertexIndex];

    vec4 worldPosition = currentInstance.transform * vec4(position, 1.0f);

    vec4 currentPosition = Constants.Scene.currentMatrices.projectionView         * worldPosition;
    gl_Position          = Constants.Scene.currentMatrices.jitteredProjectionView * worldPosition;

    vec4 previousPosition = Constants.Scene.previousMatrices.projectionView *
                            (previousInstance.transform * vec4(position, 1.0f));

    Output.currentPosition  = currentPosition.xyw;
    Output.previousPosition = previousPosition.xyw;

    Output.uv[0]  = uvs.uv[0];
    Output.uv[1]  = uvs.uv[1];

    Output.drawID = currentInstance.meshIndex;

    Output.N = normalize(currentInstance.normalMatrix * vertex.normal);
    Output.T = normalize(currentInstance.transform * vec4(vertex.tangent.xyz, 0.0f)).xyz;
    Output.T = normalize(Output.T - dot(Output.T, Output.N) * Output.N);
    Output.B = normalize(cross(Output.N, Output.T)) * vertex.tangent.w;
}