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

#ifndef INSTANCE_GLSL
#define INSTANCE_GLSL

struct Instance
{
    mat4  transform;
    mat4  normalMatrix;
    uvec4 textureIDs;
};

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer InstanceBuffer
{
    Instance instances[];
};

#endif