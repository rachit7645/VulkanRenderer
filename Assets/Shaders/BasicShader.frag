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

// Fragment inputs
layout(location = 0) in vec2 fragTexCoords;
layout(location = 1) in vec3 fragNormal;

// Texture sampler
layout(binding = 1, set = 1) uniform sampler textureSampler;
// Texture
layout(binding = 2, set = 2) uniform texture2D albedo;

// Fragment outputs
layout(location = 0) out vec4 outColor;

// Fragment entry point
void main()
{
    // Output color
    outColor = vec4(texture(sampler2D(albedo, textureSampler), fragTexCoords).rgb, 1.0f);
    // outColor = vec4(vec3(fragTexCoords * fragNormal.xy, fragNormal.z), 1.0f);
}