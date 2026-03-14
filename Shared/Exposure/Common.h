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

#ifndef EXPOSURE_COMMON_H
#define EXPOSURE_COMMON_H

#include "GLSL.h"

GLSL_NAMESPACE_BEGIN(Renderer::Exposure)

GLSL_CONSTEXPR u32 HISTOGRAM_SIZE_X = 16;
GLSL_CONSTEXPR u32 HISTOGRAM_SIZE   = HISTOGRAM_SIZE_X * HISTOGRAM_SIZE_X;

#ifndef __cplusplus

layout(buffer_reference, scalar, buffer_reference_align = 4) buffer HistogramBuffer
{
    uint bins[];
};

layout(buffer_reference, scalar, buffer_reference_align = 4) buffer LuminanceBuffer
{
    float values[];
};

#endif

GLSL_NAMESPACE_END

#endif