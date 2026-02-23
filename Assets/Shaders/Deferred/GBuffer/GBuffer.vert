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

layout(location = 0) out noperspective vec4 fragCurrentPosition;
layout(location = 1) out noperspective vec4 fragPreviousPosition;
layout(location = 2) out               vec2 fragUV[2];
layout(location = 4) out               mat3 fragTBNMatrix;
layout(location = 7) out flat          uint fragDrawID;

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

    fragCurrentPosition = Constants.Scene.currentMatrices.projectionView         * worldPosition;
    gl_Position         = Constants.Scene.currentMatrices.jitteredProjectionView * worldPosition;

    fragPreviousPosition = Constants.Scene.previousMatrices.projectionView *
                           previousInstance.transform * vec4(position, 1.0f);

    fragUV[0]  = uvs.uv[0];
    fragUV[1]  = uvs.uv[1];
    fragDrawID = currentInstance.meshIndex;

    vec3 N = normalize(currentInstance.normalMatrix * vertex.normal);
    vec3 T = normalize(currentInstance.transform * vec4(vertex.tangent.xyz, 0.0f)).xyz;
         T = normalize(T - dot(T, N) * N);
    vec3 B = normalize(cross(N, T)) * vertex.tangent.w;

    fragTBNMatrix = mat3(T, B, N);
}