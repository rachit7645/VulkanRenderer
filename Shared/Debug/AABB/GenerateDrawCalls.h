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

#ifndef AABB_DRAW_CALL_DEBUG_H
#define AABB_DRAW_CALL_DEBUG_H

#include "GLSL.h"
#include "GPU/Mesh.h"

#ifndef __cplusplus
#include "DrawCall.glsl"
#endif

GLSL_NAMESPACE_BEGIN(Renderer::Debug::AABB::Generate)

GLSL_PUSH_CONSTANT_BEGIN
{
    GLSL_BUFFER_POINTER(DrawCallBuffer) CulledOpaqueDrawCalls;
    GLSL_BUFFER_POINTER(DrawCallBuffer) CulledOpaqueDoubleSidedDrawCalls;
    GLSL_BUFFER_POINTER(DrawCallBuffer) CulledAlphaMaskedDrawCalls;
    GLSL_BUFFER_POINTER(DrawCallBuffer) CulledAlphaMaskedDoubleSidedDrawCalls;

    GLSL_BUFFER_POINTER(DrawCallBufferWithoutCount) DebugAABBDrawCalls;
} GLSL_PUSH_CONSTANT_END;

GLSL_NAMESPACE_END

#endif