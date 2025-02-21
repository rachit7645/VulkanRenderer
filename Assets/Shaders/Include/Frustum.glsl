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

#ifndef FRUSTUM_GLSL
#define FRUSTUM_GLSL

struct Plane
{
    vec3  normal;
    float dist;
};

layout(buffer_reference, scalar) readonly buffer FrustumBuffer
{
    mat4  projection;
    mat4  view;
    Plane planes;
};

#endif