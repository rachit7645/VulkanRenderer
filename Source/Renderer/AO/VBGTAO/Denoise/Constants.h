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

#ifndef VBGTAO_SPACIAL_DENOISE_CONSTANTS_H
#define VBGTAO_SPACIAL_DENOISE_CONSTANTS_H

#include "Util/Util.h"

namespace Renderer::AO::VBGTAO::Denoise
{
    struct PushConstant
    {
        u32 pointSamplerIndex;
        u32 depthDifferencesIndex;
        u32 noisyAOIndex;
        u32 outAOIndex;
        f32 finalValuePower;
    };
}

#endif
