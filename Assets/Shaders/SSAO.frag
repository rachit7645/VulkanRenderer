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

#include "Constants/SSAO.glsl"
#include "MegaSet.glsl"
#include "Packing.glsl"

layout(location = 0) in vec2 fragUV;

layout(location = 0) out float outSSAO;

vec3 GetViewPosition(vec2 uv);

void main()
{
    vec4 gNormal_Rgh_Mtl = texture(sampler2D(Textures[Constants.GNormalIndex], Samplers[Constants.GBufferSamplerIndex]), fragUV);

    vec3 normal = UnpackNormal(gNormal_Rgh_Mtl.rg);
         normal = Constants.Scene.normalView * normal;
         normal = normalize(normal);

    vec3 viewPosition = GetViewPosition(fragUV);

    vec2 screenSize   = textureSize(sampler2D(Textures[Constants.SceneDepthIndex], Samplers[Constants.GBufferSamplerIndex]), 0);
    vec2 noiseSize    = textureSize(sampler2D(Textures[Constants.NoiseIndex], Samplers[Constants.NoiseSamplerIndex]), 0);
    vec2 noiseScale   = screenSize / noiseSize;
    vec3 randomVector = normalize(vec3(texture(sampler2D(Textures[Constants.NoiseIndex], Samplers[Constants.NoiseSamplerIndex]), fragUV * noiseScale).xy, 0.0f));

    vec3 tangent   = normalize(randomVector - normal * dot(randomVector, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN       = mat3(tangent, bitangent, normal);

    float occlusion = 0.0f;

    for(uint i = 0; i < Constants.Samples.sampleCount; ++i)
    {
        vec3 samplePosition = TBN * Constants.Samples.samples[i];
             samplePosition = viewPosition + samplePosition * Constants.Radius;
        
        vec4 offset      = vec4(samplePosition, 1.0f);
             offset      = Constants.Scene.projection * offset;
             offset.xyz /= offset.w;
             offset.xyz  = offset.xyz * 0.5f + 0.5f;

        float sampleDepth = GetViewPosition(offset.xy).z;

        float rangeCheck = smoothstep(0.0f, 1.0f, Constants.Radius / abs(viewPosition.z - sampleDepth));

        occlusion += (sampleDepth >= samplePosition.z + Constants.Bias ? 1.0f : 0.0f) * rangeCheck;
    }

    occlusion = 1.0f - (occlusion / Constants.Samples.sampleCount);

    outSSAO = clamp(pow(occlusion, Constants.Power), 0.0f, 1.0f);
}

vec3 GetViewPosition(vec2 uv)
{
    vec4  gDepth            = texture(sampler2D(Textures[Constants.SceneDepthIndex], Samplers[Constants.GBufferSamplerIndex]), uv);
    float depth             = gDepth.r;
    vec4  projectedPosition = Constants.Scene.inverseProjection * vec4(uv * 2.0f - 1.0f, depth, 1.0f);
    vec3  viewPosition      = projectedPosition.xyz / projectedPosition.w;

    return viewPosition;
}