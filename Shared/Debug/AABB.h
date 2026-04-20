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

#ifndef DEBUG_AABB_H
#define DEBUG_AABB_H

#include "GLSL.h"
#include "GPU/Scene.h"
#include "GPU/Mesh.h"

#ifndef __cplusplus
#include "DrawCall.glsl"
#endif

GLSL_NAMESPACE_BEGIN(Renderer::Debug::AABB)

GLSL_PUSH_CONSTANT_BEGIN
{
    GLSL_BUFFER_POINTER(SceneBuffer)         Scene;
    GLSL_BUFFER_POINTER(MeshBuffer)          Meshes;
    GLSL_BUFFER_POINTER(InstanceBuffer)      Instances;
    GLSL_BUFFER_POINTER(InstanceIndexBuffer) InstanceIndices;
} GLSL_PUSH_CONSTANT_END;

GLSL_NAMESPACE_END

#endif