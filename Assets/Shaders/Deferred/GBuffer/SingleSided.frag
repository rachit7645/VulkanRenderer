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

#include "Packing.glsl"
#include "MegaSet.glsl"
#include "PBR.glsl"
#include "Deferred/GBuffer.h"

layout(location = 0) in      vec4 fragCurrentPosition;
layout(location = 1) in      vec4 fragPreviousPosition;
layout(location = 2) in      vec2 fragUV[2];
layout(location = 4) in      mat3 fragTBNMatrix;
layout(location = 7) in flat uint fragDrawID;

layout(location = 0) out vec4 gAlbedoIoR;
layout(location = 1) out vec2 gNormal;
layout(location = 2) out vec2 gRoughnessMetallic;
layout(location = 3) out vec3 gEmmisive;
layout(location = 4) out vec2 gMotionVectors;

void main()
{
    Mesh mesh = Constants.CurrentMeshes.meshes[fragDrawID];

    vec3 albedo  = texture(sampler2D(Textures[mesh.material.albedoID], Samplers[Constants.TextureSamplerIndex]), fragUV[mesh.material.albedoUVMapID]).rgb;
         albedo *= mesh.material.albedoFactor.rgb;

    gAlbedoIoR.rgb = albedo.rgb;
    gAlbedoIoR.a   = PackIoR(mesh.material.ior);

    vec3 normal = texture(sampler2D(Textures[mesh.material.normalID], Samplers[Constants.TextureSamplerIndex]), fragUV[mesh.material.normalUVMapID]).rgb;
         normal = GetNormalFromMap(normal, fragTBNMatrix);

    gNormal = PackNormal(normal);

    vec3 aoRghMtl    = texture(sampler2D(Textures[mesh.material.aoRghMtlID], Samplers[Constants.TextureSamplerIndex]), fragUV[mesh.material.aoRghMtlUVMapID]).rgb;
         aoRghMtl.g *= mesh.material.roughnessFactor;
         aoRghMtl.b *= mesh.material.metallicFactor;

    gRoughnessMetallic.r = aoRghMtl.g;
    gRoughnessMetallic.g = aoRghMtl.b;

    vec3 emmisive  = texture(sampler2D(Textures[mesh.material.emmisiveID], Samplers[Constants.TextureSamplerIndex]), fragUV[mesh.material.emmisiveUVMapID]).rgb;
         emmisive *= mesh.material.emmisiveFactor;
         emmisive *= mesh.material.emmisiveStrength;

    gEmmisive = emmisive;

    vec2 currentUV  = (fragCurrentPosition.xy  / fragCurrentPosition.w ) * 0.5f + 0.5f;
    vec2 previousUV = (fragPreviousPosition.xy / fragPreviousPosition.w) * 0.5f + 0.5f;

    gMotionVectors = currentUV - previousUV;
}