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

// Vertex inputs
layout(location = 0) in vec2 position;

// Vertex outputs
layout(location = 0) out vec2 fragTexCoords;

void main()
{
    // Set position
    gl_Position = vec4(position.xy, 0.0f, 1.0f);
    // Set texture coords
    fragTexCoords = 0.5f * (position + vec2(1.0));
}