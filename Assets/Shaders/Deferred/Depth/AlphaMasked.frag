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

#include "MegaSet.glsl"
#include "Deferred/Depth/AlphaMasked.h"

layout(location = 0) in      vec2 fragUV;
layout(location = 1) in flat uint fragDrawID;

void main()
{
    Mesh mesh = Constants.Meshes.meshes[fragDrawID];

    float alpha  = texture(sampler2D(Textures[mesh.material.albedoID], Samplers[Constants.TextureSamplerIndex]), fragUV).a;
          alpha *= mesh.material.albedoFactor.a;

    if (alpha < mesh.material.alphaCutOff)
    {
        discard;
    }
}