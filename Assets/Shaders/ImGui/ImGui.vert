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

#include "Converter.glsl"
#include "ImGui/DearImGui.h"

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragUV;

void main()
{
    Vertex vertex = Constants.Vertices.vertices[gl_VertexIndex];

    fragColor = UnpackRGBA8(vertex.color);
    fragUV    = vertex.uv;

    gl_Position = vec4(vertex.position * Constants.Scale + Constants.Translate, 0.0f, 1.0f);
}