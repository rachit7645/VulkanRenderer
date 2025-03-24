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

#ifndef FORWARD_PUSH_CONSTANT
#define FORWARD_PUSH_CONSTANT

#include "Mesh.glsl"
#include "Scene.glsl"
#include "Vertex.glsl"
#include "CSM.glsl"
#include "PointShadowMap.glsl"
#include "SpotShadowMap.glsl"

layout(push_constant, scalar) uniform ConstantsBuffer
{
    SceneBuffer       Scene;
    MeshBuffer        Meshes;
    VisibleMeshBuffer VisibleMeshes;
    PositionBuffer    Positions;
    VertexBuffer      Vertices;
    CascadeBuffer     Cascades;
    PointShadowBuffer PointShadows;
    SpotShadowBuffer  SpotShadows;

    uint TextureSamplerIndex;
    uint IBLSamplerIndex;
    uint ShadowSamplerIndex;

    uint IrradianceIndex;
    uint PreFilterIndex;
    uint BRDFLUTIndex;
    uint ShadowMapIndex;
    uint PointShadowMapIndex;
    uint SpotShadowMapIndex;
} Constants;

#endif