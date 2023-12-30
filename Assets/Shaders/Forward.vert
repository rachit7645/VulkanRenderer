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

// Extensions
#extension GL_GOOGLE_include_directive : enable

// Includes
#include "Lights.glsl"

layout(binding = 0, set = 0) uniform SceneBuffer
{
    // Matrices
    mat4 projection;
    mat4 view;
    // Camera
    vec4 cameraPos;
    // Lights
    DirLight light;
} Scene;

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
layout(location = 3) in vec3 tangent;

// Vertex outputs
layout(location = 0) out vec3 fragPosition;
layout(location = 1) out vec2 fragTexCoords;
layout(location = 2) out vec3 fragToCamera;
layout(location = 3) out mat3 fragTBNMatrix;

// Vertex entry point
void main()
{
    // Transform vertex by model matrix
    vec4 fragPos = Constants.transform * vec4(position, 1.0f);
    // Set world position
    fragPosition = fragPos.xyz;
    // Transform from world to clip space
    gl_Position = Scene.projection * Scene.view * fragPos;

    // Pass through texture coords
    fragTexCoords = texCoords;

    // Transform normal
    vec3 N = normalize(mat3(Constants.normalMatrix) * normal);
    // Transform tangent
    vec3 T = normalize(mat3(Constants.normalMatrix) * tangent);
    // Re-orthagonalize tangent
    T = normalize(T - dot(T, N) * N);
    // Calculate bitangent
    vec3 B = cross(N, T);
    // Compute TBN matrix
    fragTBNMatrix = mat3(T, B, N);

    // Calculate to camera vector
    fragToCamera = normalize(Scene.cameraPos.xyz - fragPosition);
}