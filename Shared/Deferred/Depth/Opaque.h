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

#ifndef DEPTH_PUSH_CONSTANT
#define DEPTH_PUSH_CONSTANT

#include "GLSL.h"
#include "GPU/Mesh.h"
#include "GPU/Vertex.h"
#include "GPU/Scene.h"

#ifndef __cplusplus
#include "DrawCall.glsl"
#endif

GLSL_NAMESPACE_BEGIN(Renderer::Depth::Opaque)

GLSL_PUSH_CONSTANT_BEGIN
{
    GLSL_BUFFER_POINTER(SceneBuffer)     Scene;
    GLSL_BUFFER_POINTER(MeshBuffer)      Meshes;
    GLSL_BUFFER_POINTER(MeshIndexBuffer) MeshIndices;
    GLSL_BUFFER_POINTER(PositionBuffer)  Positions;
} GLSL_PUSH_CONSTANT_END;

GLSL_NAMESPACE_END

#endif