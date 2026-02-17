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

#ifndef EXPOSURE_COMMON_GLSL_H
#define EXPOSURE_COMMON_GLSL_H

#include "GLSL.h"
#include "Common.h"

GLSL_NAMESPACE_BEGIN(Renderer::Exposure::Average)

GLSL_PUSH_CONSTANT_BEGIN
{
    GLSL_BUFFER_POINTER(HistogramBuffer) Histogram;
    GLSL_BUFFER_POINTER(LuminanceBuffer) Luminance;

    u32 PixelCount;
    f32 TimeCoefficient;
    f32 ExposureBias;
    u32 CurrentFrame;
    u32 PreviousFrame;
} GLSL_PUSH_CONSTANT_END;

GLSL_NAMESPACE_END

#endif