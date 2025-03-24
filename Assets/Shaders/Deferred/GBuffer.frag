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

#include "Constants/Deferred/GBuffer.glsl"
#include "Packing.glsl"
#include "MegaSet.glsl"

layout(location = 0) in      vec4 currentPosition;
layout(location = 1) in      vec2 fragTexCoords;
layout(location = 2) in flat uint fragDrawID;
layout(location = 3) in      mat3 fragTBNMatrix;

layout(location = 0) out vec3 gAlbedo;
layout(location = 1) out vec4 gNormal_Rgh_Mtl;
layout(location = 2) out vec2 motionVectors;

void main()
{
    Mesh mesh = Constants.Meshes.meshes[fragDrawID];

    vec4 albedo  = texture(sampler2D(Textures[mesh.material.albedo], Samplers[Constants.TextureSamplerIndex]), fragTexCoords);
    albedo.rgb  *= mesh.material.albedoFactor.rgb;

    gAlbedo = albedo.rgb;

    vec3 normal = texture(sampler2D(Textures[mesh.material.normal], Samplers[Constants.TextureSamplerIndex]), fragTexCoords).rgb;
    normal      = GetNormalFromMap(normal, fragTBNMatrix);

    vec3 aoRghMtl = texture(sampler2D(Textures[mesh.material.aoRghMtl], Samplers[Constants.TextureSamplerIndex]), fragTexCoords).rgb;
    aoRghMtl.g   *= mesh.material.roughnessFactor;
    aoRghMtl.b   *= mesh.material.metallicFactor;

    gNormal_Rgh_Mtl.rg = PackNormal(normal);
    gNormal_Rgh_Mtl.b  = aoRghMtl.g;
    gNormal_Rgh_Mtl.a  = aoRghMtl.b;

    ivec2 depthSize = textureSize(sampler2D(Textures[Constants.PreviousDepthIndex], Samplers[Constants.DepthSamplerIndex]), 0);
    vec2  screenUV  = (vec2(gl_FragCoord.xy) + 0.5f) / vec2(depthSize);

    float previousDepth       = texture(sampler2D(Textures[Constants.PreviousDepthIndex], Samplers[Constants.DepthSamplerIndex]), screenUV).r;
    vec4  reprojectedPosition = GetClipPosition(Constants.Scene.previousMatrices, screenUV, previousDepth);

    vec2 current  = (currentPosition.xy / currentPosition.w) * 0.5f + 0.5f;
    vec2 previous = reprojectedPosition.xy * 0.5f + 0.5f;

    motionVectors = current - previous;
}