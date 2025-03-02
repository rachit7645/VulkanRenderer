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
#extension GL_EXT_nonuniform_qualifier : enable

#include "Constants/GBuffer.glsl"
#include "Packing.glsl"
#include "MegaSet.glsl"

layout(location = 0) in      vec2 fragTexCoords;
layout(location = 1) in flat uint fragDrawID;
layout(location = 2) in      mat3 fragTBNMatrix;

layout(location = 0) out vec3 gAlbedo;
layout(location = 1) out vec4 gNormal_Rgh_Mtl;

void main()
{
    Mesh mesh = Constants.Meshes.meshes[fragDrawID];

    vec4 albedo  = texture(sampler2D(Textures[mesh.material.albedo], Samplers[Constants.TextureSamplerIndex]), fragTexCoords);
    albedo.rgb  *= mesh.material.albedoFactor.rgb;

    vec3 normal = texture(sampler2D(Textures[mesh.material.normal], Samplers[Constants.TextureSamplerIndex]), fragTexCoords).rgb;
    normal      = GetNormalFromMap(normal, fragTBNMatrix);

    vec3 aoRghMtl = texture(sampler2D(Textures[mesh.material.aoRghMtl], Samplers[Constants.TextureSamplerIndex]), fragTexCoords).rgb;
    aoRghMtl.g   *= mesh.material.roughnessFactor;
    aoRghMtl.b   *= mesh.material.metallicFactor;

    gAlbedo = albedo.rgb;

    gNormal_Rgh_Mtl.rg = PackNormal(normal);
    gNormal_Rgh_Mtl.b  = aoRghMtl.g;
    gNormal_Rgh_Mtl.a  = aoRghMtl.b;
}