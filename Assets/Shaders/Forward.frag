/*
 *    Copyright 2023 - 2024 Rachit Khandelwal
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
#include "Material.glsl"
#include "Texture.glsl"
#include "Lights.glsl"
#include "PBR.glsl"
#include "Instance.glsl"
#include "Scene.glsl"

// Fragment inputs
layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec2 fragTexCoords;
layout(location = 2) in vec3 fragToCamera;
layout(location = 3) in mat3 fragTBNMatrix;

// Fragment outputs
layout(location = 0) out vec4 outColor;

layout(push_constant, scalar) uniform ConstantsBuffer
{
    SceneBuffer    Scene;
    InstanceBuffer Instances;
} Constants;

layout(set = 0, binding = 0) uniform sampler texSampler;
layout(set = 1, binding = 0) uniform texture2D textures[];

void main()
{
    // This is assumed to be an SRGB texture
    vec3 albedo   = SampleLinear(textures[0], texSampler, fragTexCoords);
    vec3 normal   = GetNormalFromMap(Sample(textures[1], texSampler, fragTexCoords), fragTBNMatrix);
    vec3 aoRghMtl = Sample(textures[2], texSampler, fragTexCoords);

    vec3 F0 = mix(vec3(0.04f), albedo, aoRghMtl.b);
    vec3 Lo = CalculateLight(GetDirLightInfo(Constants.Scene.light), normal, fragToCamera, F0, albedo, aoRghMtl.g, aoRghMtl.b);
         Lo += albedo * vec3(0.03f);

    outColor = vec4(Lo, 1.0f);
}