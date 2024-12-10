/*
 *   Copyright 2023 - 2024 Rachit Khandelwal
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#version 460

// Extensions
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference     : enable
#extension GL_EXT_scalar_block_layout  : enable

// Includes
#include "Lights.glsl"
#include "ForwardPushConstant.glsl"

// Vertex outputs
layout(location = 0)      out vec3 fragPosition;
layout(location = 1)      out vec2 fragTexCoords;
layout(location = 2)      out vec3 fragToCamera;
layout(location = 3) flat out uint fragDrawID;
layout(location = 4)      out mat3 fragTBNMatrix;

void main()
{
    Vertex   vertex   = Constants.Vertices.vertices[gl_VertexIndex];
    Instance instance = Constants.Instances.instances[gl_DrawID];

    vec4 fragPos = instance.transform * vec4(vertex.attrib0.xyz, 1.0f);
    fragPosition = fragPos.xyz;
    gl_Position  = Constants.Scene.projection * Constants.Scene.view * fragPos;

    fragTexCoords = vec2(vertex.attrib0.w, vertex.attrib1.x);
    fragToCamera  = normalize(Constants.Scene.cameraPos.xyz - fragPosition);
    fragDrawID    = gl_DrawID;

    vec3 N = normalize(mat3(instance.normalMatrix) * vertex.attrib1.yzw);
    vec3 T = normalize(mat3(instance.normalMatrix) * vertex.attrib2.xyz);
         T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);

    fragTBNMatrix = mat3(T, B, N);
}