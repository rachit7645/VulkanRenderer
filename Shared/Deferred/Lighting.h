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

#ifndef LIGHTING_PUSH_CONSTANT
#define LIGHTING_PUSH_CONSTANT

#include "GLSL.h"
#include "GPU/Scene.h"

GLSL_NAMESPACE_BEGIN(Renderer::Lighting)

GLSL_PUSH_CONSTANT_BEGIN
{
    GLSL_BUFFER_POINTER(SceneBuffer) Scene;

    u32 GBufferSamplerIndex;
    u32 IBLSamplerIndex;
    u32 ShadowSamplerIndex;

    u32 GAlbedoIndex;
    u32 GNormalIndex;
    u32 SceneDepthIndex;

    u32 IrradianceIndex;
    u32 PreFilterIndex;
    u32 BRDFLUTIndex;

    u32 ShadowMapIndex;
    u32 PointShadowMapIndex;
    u32 SpotShadowMapIndex;

    u32 AOIndex;
} GLSL_PUSH_CONSTANT_END;

GLSL_NAMESPACE_END

#endif