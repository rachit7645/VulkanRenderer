/*
 * Copyright (c) 2023 - 2026 Rachit
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
#include "Encoding.glsl"

layout(location = 0) out VertexData
{
    vec3 position;

    vec2 uv[2];

    vec3 N;
    vec3 T;

    float TangentSign;

    // (x, y, w) 
    noperspective vec3 currentPosition;
    noperspective vec3 previousPosition;

    flat uint drawID;
} Output;

void main()
{
    // TODO: This code assumes that instanceIndex will not change from one frame to another, which doesn't sound plausible
    uint     instanceIndex    = Constants.InstanceIndices.indices[gl_DrawID];
    Instance currentInstance  = Constants.CurrentInstances.instances[instanceIndex];
    Instance previousInstance = Constants.PreviousInstances.instances[instanceIndex];

    vec3   position = Constants.Positions.positions[gl_VertexIndex];
    UV     uvs      = Constants.UVs.uvs[gl_VertexIndex];
    Vertex vertex   = Constants.Vertices.vertices[gl_VertexIndex];

    vec4 worldPosition = currentInstance.transform * vec4(position, 1.0f);

    Output.position = worldPosition.xyz;

    vec4 currentPosition = Constants.Scene.currentMatrices.projectionView         * worldPosition;
    gl_Position          = Constants.Scene.currentMatrices.jitteredProjectionView * worldPosition;

    vec4 previousPosition = Constants.Scene.previousMatrices.projectionView *
                            (previousInstance.transform * vec4(position, 1.0f));

    Output.currentPosition  = currentPosition.xyw;
    Output.previousPosition = previousPosition.xyw;

    Output.uv[0] = uvs.uv[0];
    Output.uv[1] = uvs.uv[1];

    Output.drawID = currentInstance.meshIndex;

    vec3 normal  = UnpackNormalFromVertex(vertex.normal);
    vec4 tangent = UnpackTangentFromVertex(vertex.tangent);

    Output.N = normalize(currentInstance.normalMatrix * normal);
    Output.T = normalize(currentInstance.transform * vec4(tangent.xyz, 0.0f)).xyz;

    Output.TangentSign = tangent.w;
}