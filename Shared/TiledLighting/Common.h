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

#ifndef TILES_COMMON_H
#define TILES_COMMON_H

#include "GLSL.h"
#include "GPU/Lights.h"

GLSL_NAMESPACE_BEGIN(Renderer::TiledLighting)

GLSL_CONSTEXPR u32 TILE_SIZE = 16;

#ifdef __cplusplus

using GPU::MAX_POINT_LIGHT_COUNT;
using GPU::MAX_SHADOWED_POINT_LIGHT_COUNT;
using GPU::MAX_SPOT_LIGHT_COUNT;
using GPU::MAX_SHADOWED_SPOT_LIGHT_COUNT;

#endif

struct TileLightIndices
{
    u16 pointLightCount;
    u16 pointLightIndices[MAX_POINT_LIGHT_COUNT];

    u16 shadowedPointLightCount;
    u16 shadowedPointLightIndices[MAX_SHADOWED_POINT_LIGHT_COUNT];

    u16 spotLightCount;
    u16 spotLightIndices[MAX_SPOT_LIGHT_COUNT];

    u16 shadowedSpotLightCount;
    u16 shadowedSpotLightIndices[MAX_SHADOWED_SPOT_LIGHT_COUNT];
};

#ifndef __cplusplus

layout(buffer_reference, scalar, buffer_reference_align = 4) buffer TileLightIndexBuffer
{
    TileLightIndices indices[];
};

#endif

GLSL_NAMESPACE_END

#endif