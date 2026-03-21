/*
 * Copyright (c) 2023 - 2026 Rachit
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

layout(location = 0) in VertexData
{
    vec3 position;

    vec2 uv[2];

    vec3 N;
    vec3 T;
    vec3 B;

    noperspective vec3 currentPosition;
    noperspective vec3 previousPosition;

    flat uint drawID;
} Input;

layout(location = 0) out vec4 gAlbedoIoR;
layout(location = 1) out vec2 gNormal;
layout(location = 2) out vec3 gRoughnessMetallicHorizon;
layout(location = 3) out vec3 gEmmisive;
layout(location = 4) out vec2 gMotionVectors;

void main()
{
    Mesh mesh = Constants.CurrentMeshes.meshes[Input.drawID];

    vec3 albedo  = texture(sampler2D(Textures[nonuniformEXT(mesh.material.albedoID)], Samplers[Constants.TextureSamplerIndex]), Input.uv[mesh.material.albedoUVMapID]).rgb;
         albedo *= mesh.material.albedoFactor.rgb;

    gAlbedoIoR.rgb = albedo.rgb;
    gAlbedoIoR.a   = PackIoR(mesh.material.ior);

    mat3 TBN = mat3(Input.T, Input.B, Input.N);

    vec3 normal = texture(sampler2D(Textures[nonuniformEXT(mesh.material.normalID)], Samplers[Constants.TextureSamplerIndex]), Input.uv[mesh.material.normalUVMapID]).rgb;
         normal = GetNormalFromMap(normal, TBN);

    if (!gl_FrontFacing)
    {
        normal = -normal;
    }

    gNormal = PackNormal(normal);

    vec3 aoRghMtl    = texture(sampler2D(Textures[nonuniformEXT(mesh.material.aoRghMtlID)], Samplers[Constants.TextureSamplerIndex]), Input.uv[mesh.material.aoRghMtlUVMapID]).rgb;
         aoRghMtl.g *= mesh.material.roughnessFactor;
         aoRghMtl.b *= mesh.material.metallicFactor;

    vec3 toCamera  = normalize(Constants.Scene.cameraPosition - Input.position);
    vec3 reflected = normalize(reflect(-toCamera, normal));

    gRoughnessMetallicHorizon.r = aoRghMtl.g;
    gRoughnessMetallicHorizon.g = aoRghMtl.b;
    gRoughnessMetallicHorizon.b = CalculateHorizonOcclusion(reflected, Input.N);

    vec3 emmisive  = texture(sampler2D(Textures[nonuniformEXT(mesh.material.emmisiveID)], Samplers[Constants.TextureSamplerIndex]), Input.uv[mesh.material.emmisiveUVMapID]).rgb;
         emmisive *= mesh.material.emmisiveFactor;
         emmisive *= mesh.material.emmisiveStrength;

    gEmmisive = emmisive;

    vec2 currentUV  = (Input.currentPosition.xy  / Input.currentPosition.z ) * 0.5f + 0.5f;
    vec2 previousUV = (Input.previousPosition.xy / Input.previousPosition.z) * 0.5f + 0.5f;

    gMotionVectors = currentUV - previousUV;
}