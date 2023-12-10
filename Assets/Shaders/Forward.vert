/*
 *   Copyright 2023 Rachit Khandelwal
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

layout(binding = 0) uniform SharedDataBuffer
{
    mat4 proj;
    mat4 view;
} Shared;

// Push constant buffer
layout(push_constant) uniform ConstantsBuffer
{
    // Model matrix
    mat4 transform;
    // Normal matrix
    mat4 normalMatrix;
} Constants;

// Vertex inputs
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoords;
layout(location = 2) in vec3 normal;

// Vertex outputs
layout(location = 0) out vec2 fragTexCoords;
layout(location = 1) out vec3 fragNormal;

// Vertex entry point
void main()
{
    // Set position
    gl_Position = Shared.proj * Shared.view * Constants.transform * vec4(position, 1.0f);
    // Set texture coords
    fragTexCoords = texCoords;
    // Set normal
    fragNormal = normalize(Constants.normalMatrix * vec4(normal, 0.0f)).xyz;
}