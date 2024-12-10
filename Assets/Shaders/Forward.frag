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
#extension GL_EXT_nonuniform_qualifier : enable

// Includes
#include "Material.glsl"
#include "Texture.glsl"
#include "Lights.glsl"
#include "PBR.glsl"
#include "Instance.glsl"
#include "Scene.glsl"
#include "ForwardPushConstant.glsl"

// Fragment inputs
layout(location = 0)      in vec3 fragPosition;
layout(location = 1)      in vec2 fragTexCoords;
layout(location = 2)      in vec3 fragToCamera;
layout(location = 3) flat in uint fragDrawID;
layout(location = 4)      in mat3 fragTBNMatrix;

// Fragment outputs
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler texSampler;
layout(set = 1, binding = 0) uniform texture2D textures[];

void main()
{
    uvec4 textureIDs = Constants.Instances.instances[fragDrawID].textureIDs;

    // This is assumed to be an SRGB texture
    vec3 albedo   = SampleLinear(textures[textureIDs.x], texSampler, fragTexCoords);
    vec3 normal   = GetNormalFromMap(Sample(textures[textureIDs.y], texSampler, fragTexCoords), fragTBNMatrix);
    vec3 aoRghMtl = Sample(textures[textureIDs.z], texSampler, fragTexCoords);

    vec3 F0 = mix(vec3(0.04f), albedo, aoRghMtl.b);
    volatile vec3 Lo = CalculateLight(GetDirLightInfo(Constants.Scene.light), normal, fragToCamera, F0, albedo, aoRghMtl.g, aoRghMtl.b);
         Lo += albedo * vec3(0.03f);

    outColor = vec4(albedo, 1.0f);
}