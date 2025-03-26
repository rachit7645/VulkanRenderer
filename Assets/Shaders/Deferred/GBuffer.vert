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

#include "Constants/Deferred/GBuffer.glsl"
#include "Material.glsl"

layout(location = 0) out      vec4 currentPosition;
layout(location = 1) out      vec2 fragTexCoords;
layout(location = 2) out flat uint fragDrawID;
layout(location = 3) out      mat3 fragTBNMatrix;

void main()
{
    uint   meshIndex = Constants.VisibleMeshes.indices[gl_DrawID];
    Mesh   mesh      = Constants.Meshes.meshes[meshIndex];
    vec3   position  = Constants.Positions.positions[gl_VertexIndex];
    Vertex vertex    = Constants.Vertices.vertices[gl_VertexIndex];

    vec4 worldPosition       = mesh.transform                       * vec4(position, 1.0f);
    vec4 currentViewPosition = Constants.Scene.currentMatrices.view * worldPosition;

    currentPosition = Constants.Scene.currentMatrices.projection         * currentViewPosition;
    gl_Position     = Constants.Scene.currentMatrices.jitteredProjection * currentViewPosition;

    fragTexCoords = vertex.uv0;
    fragDrawID    = meshIndex;

    vec3 N = normalize(mesh.normalMatrix * vertex.normal);
    vec3 T = normalize(mesh.transform    * vec4(vertex.tangent.xyz, 0.0f)).xyz;
         T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T) * vertex.tangent.w;

    fragTBNMatrix = mat3(T, B, N);
}