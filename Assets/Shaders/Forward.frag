/*
 *    Copyright 2023 Rachit Khandelwal
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
#include "Material.glsl"
#include "Texture.glsl"
#include "Lights.glsl"
#include "PBR.glsl"
#include "ACES.glsl"

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

// Fragment inputs
layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec2 fragTexCoords;
layout(location = 2) in vec3 fragToCamera;
layout(location = 3) in mat3 fragTBNMatrix;

// Texture sampler
layout(binding = 1, set = 1) uniform sampler texSampler;
// Textures
layout(binding = 2, set = 2) uniform texture2D textures[];

// Fragment outputs
layout(location = 0) out vec4 outColor;

void main()
{
    // Get linear albedo
    vec3 albedo = SampleLinear(ALBEDO(textures), texSampler, fragTexCoords);
    // Calculate normal
    vec3 normal = GetNormalFromMap(Sample(NORMAL(textures), texSampler, fragTexCoords), fragTBNMatrix);
    // Get materials
    vec3 aoRghMtl = Sample(AO_RGH_MTL(textures), texSampler, fragTexCoords);

    // Calculate F0
    vec3 F0 = mix(vec3(0.04f), albedo, aoRghMtl.b);
    // Calculate light
    vec3 Lo = CalculateLight(GetDirLightInfo(Scene.light), normal, fragToCamera, F0, albedo, aoRghMtl.g, aoRghMtl.b);
    // Add ambient
    Lo += albedo * vec3(0.03f);

    // Tonemap
    Lo = ACESToneMap(Lo);
    // Output color
    outColor = vec4(Lo, 1.0f);
}