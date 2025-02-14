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

#include "Constants/Skybox.glsl"

layout(location = 0) out vec3 txCoords;

void main()
{
    // Disable translation
    mat4 rotView = mat4(mat3(Constants.Scene.view));

    txCoords    = Constants.Vertices.positions[gl_VertexIndex];
    gl_Position = Constants.Scene.projection * rotView * vec4(txCoords, 1.0f);
}