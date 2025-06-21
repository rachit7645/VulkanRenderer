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

#ifndef VBGTAO_PUSH_CONSTANT
#define VBGTAO_PUSH_CONSTANT

#include "GLSL.h"
#include "GPU/Scene.h"

GLSL_NAMESPACE_BEGIN(Renderer::AO::VBGTAO::Occlusion)

GLSL_CONSTANT(u32, GTAO_DEPTH_MIP_LEVELS, 5);
GLSL_CONSTANT(u32, GTAO_HILBERT_LEVEL,    6);
GLSL_CONSTANT(u32, GTAO_HILBERT_WIDTH,    1u << GTAO_HILBERT_LEVEL);

GLSL_PUSH_CONSTANT_BEGIN
{
    GLSL_BUFFER_POINTER(SceneBuffer) Scene;

    u32 PointSamplerIndex;
    u32 LinearSamplerIndex;

    u32 HilbertLUTIndex;
    u32 GNormalIndex;

    u32 PreFilterDepthIndex;

    u32 OutDepthDifferencesIndex;
    u32 OutNoisyAOIndex;

    u32 TemporalIndex;

    f32 Thickness;
} GLSL_PUSH_CONSTANT_END;

GLSL_NAMESPACE_END

#endif