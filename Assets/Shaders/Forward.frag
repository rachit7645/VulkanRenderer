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
#extension GL_EXT_nonuniform_qualifier : enable

// Includes
#include "Material.glsl"
#include "GammaCorrect.glsl"
#include "Lights.glsl"
#include "PBR.glsl"
#include "ForwardConstants.glsl"

// Fragment inputs
layout(location = 0) in      vec3 fragPosition;
layout(location = 1) in      vec2 fragTexCoords;
layout(location = 2) in      vec3 fragToCamera;
layout(location = 3) in flat uint fragDrawID;
layout(location = 4) in      mat3 fragTBNMatrix;

// Fragment outputs
layout(location = 0) out vec4 outColor;

// Mega set
layout(set = 0, binding = 0) uniform sampler   samplers[];
layout(set = 0, binding = 1) uniform texture2D textures[];

void main()
{
    Mesh mesh         = Constants.Meshes.meshes[fragDrawID];
    uint samplerIndex = Constants.Scene.samplerIndex;

    vec4 albedo = texture(sampler2D(textures[MAT_ALBEDO_ID], samplers[samplerIndex]), fragTexCoords);
    albedo.xyz  = ToLinear(albedo.xyz);
    albedo     *= mesh.albedoFactor;

    // Transparency check
    if (albedo.a < 0.5f) discard;

    vec3 normal = texture(sampler2D(textures[MAT_NORMAL_ID], samplers[samplerIndex]), fragTexCoords).rgb;
    normal      = GetNormalFromMap(normal, fragTBNMatrix);

    vec3 aoRghMtl = texture(sampler2D(textures[MAT_AO_RGH_MTL_ID], samplers[samplerIndex]), fragTexCoords).rgb;
    aoRghMtl.g   *= mesh.roughnessFactor;
    aoRghMtl.b   *= mesh.metallicFactor;

    vec3 Lo = CalculateLight
    (
        GetDirLightInfo(Constants.Scene.light),
        normal,
        fragToCamera,
        albedo.rgb,
        aoRghMtl.g,
        aoRghMtl.b
    );

    Lo += albedo.rgb * vec3(0.25f);

    outColor = vec4(Lo, 1.0f);
}