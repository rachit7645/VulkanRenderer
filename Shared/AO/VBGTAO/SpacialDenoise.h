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

#ifndef VBGTAO_SPACIAL_DENOISE_PUSH_CONSTANT
#define VBGTAO_SPACIAL_DENOISE_PUSH_CONSTANT

#include "GLSL.h"

GLSL_NAMESPACE_BEGIN(Renderer::AO::VBGTAO::Denoise)

GLSL_PUSH_CONSTANT_BEGIN
{
    u32 PointSamplerIndex;
    u32 DepthDifferencesIndex;
    u32 NoisyAOIndex;
    u32 OutAOIndex;
    f32 FinalValuePower;
} GLSL_PUSH_CONSTANT_END;

GLSL_NAMESPACE_END

#endif