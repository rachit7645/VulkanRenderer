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

#ifndef CULLING_DEBUG_H
#define CULLING_DEBUG_H

#include "GLSL.h"
#include "GPU/Mesh.h"
#include "GPU/Plane.h"

#ifndef __cplusplus
#include "DrawCall.glsl"
#endif

GLSL_NAMESPACE_BEGIN(Renderer::Debug::Culling)

struct MeshCullingStatistics
{
    u32 instanceCount;
    u32 vertexCount;
    u32 indexCount;
};

GLSL_SHADER_STORAGE_BUFFER(CullingStatisticsBuffer, writeonly)
{
    MeshCullingStatistics opaque;
    MeshCullingStatistics opaqueDoubleSided;
    MeshCullingStatistics alphaMasked;
    MeshCullingStatistics alphaMaskedDoubleSided;
    MeshCullingStatistics total;
};

#ifndef __cplusplus
MeshCullingStatistics ZeroMeshCullingStatistics()
{
    MeshCullingStatistics statistics;

    statistics.instanceCount = 0;
    statistics.vertexCount   = 0;
    statistics.indexCount    = 0;

    return statistics;
}
#endif

GLSL_PUSH_CONSTANT_BEGIN
{
    GLSL_BUFFER_POINTER(MeshBuffer)     Meshes;
    GLSL_BUFFER_POINTER(InstanceBuffer) Instances;

    GLSL_BUFFER_POINTER(DrawCallBuffer)      CulledOpaqueDrawCalls;
    GLSL_BUFFER_POINTER(InstanceIndexBuffer) CulledOpaqueInstanceIndices;

    GLSL_BUFFER_POINTER(DrawCallBuffer)      CulledOpaqueDoubleSidedDrawCalls;
    GLSL_BUFFER_POINTER(InstanceIndexBuffer) CulledOpaqueDoubleSidedInstanceIndices;

    GLSL_BUFFER_POINTER(DrawCallBuffer)      CulledAlphaMaskedDrawCalls;
    GLSL_BUFFER_POINTER(InstanceIndexBuffer) CulledAlphaMaskedInstanceIndices;

    GLSL_BUFFER_POINTER(DrawCallBuffer)      CulledAlphaMaskedDoubleSidedDrawCalls;
    GLSL_BUFFER_POINTER(InstanceIndexBuffer) CulledAlphaMaskedDoubleSidedInstanceIndices;

    GLSL_BUFFER_POINTER(CullingStatisticsBuffer) CullingStatistics;
} GLSL_PUSH_CONSTANT_END;

GLSL_NAMESPACE_END

#endif