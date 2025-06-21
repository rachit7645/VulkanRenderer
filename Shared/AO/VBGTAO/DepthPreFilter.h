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

#ifndef VBGTAO_DEPTH_PREFILTER_PUSH_CONSTANT
#define VBGTAO_DEPTH_PREFILTER_PUSH_CONSTANT

#include "GLSL.h"

GLSL_NAMESPACE_BEGIN(Renderer::AO::VBGTAO::DepthPreFilter)

GLSL_PUSH_CONSTANT_BEGIN
{
    u32 PointSamplerIndex;
    u32 SceneDepthIndex;

    u32 OutDepthMip0Index;
    u32 OutDepthMip1Index;
    u32 OutDepthMip2Index;
    u32 OutDepthMip3Index;
    u32 OutDepthMip4Index;
} GLSL_PUSH_CONSTANT_END;

GLSL_NAMESPACE_END

#endif