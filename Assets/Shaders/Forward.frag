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

// Fragment inputs
layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec2 fragTexCoords;
layout(location = 2) in mat3 fragTBNMatrix;

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

    // Fool GLSL
    vec3(normal + aoRghMtl + fragPosition);

    // Output color
    outColor = vec4(albedo, 1.0f);
}