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

// Extensions
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_buffer_reference     : enable
#extension GL_EXT_scalar_block_layout  : enable

#include "DepthConstants.glsl"

void main()
{
    Mesh   mesh   = Constants.Meshes.meshes[gl_DrawID];
    Vertex vertex = Constants.Vertices.vertices[gl_VertexIndex];

    vec4 fragPos = mesh.transform * vec4(vertex.position, 1.0f);
    gl_Position  = Constants.Scene.projection * Constants.Scene.view * fragPos;
}