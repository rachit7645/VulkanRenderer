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

#ifndef SKYBOX_PUSH_CONSTANT
#define SKYBOX_PUSH_CONSTANT

#include "GLSL.h"
#include "GPU/Scene.h"

GLSL_NAMESPACE_BEGIN(Renderer::Skybox)

#ifndef __cplusplus

layout(buffer_reference, scalar) readonly buffer VertexBuffer
{
    vec3 positions[];
};

#endif

GLSL_PUSH_CONSTANT_BEGIN
{
    GLSL_BUFFER_POINTER(VertexBuffer) Vertices;
    GLSL_BUFFER_POINTER(SceneBuffer)  Scene;

    u32 SamplerIndex;
    u32 CubemapIndex;
} GLSL_PUSH_CONSTANT_END;

GLSL_NAMESPACE_END

#endif