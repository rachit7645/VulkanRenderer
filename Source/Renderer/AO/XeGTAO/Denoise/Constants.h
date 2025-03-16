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

#ifndef XE_GTAO_DENOISE_CONSTANTS_H
#define XE_GTAO_DENOISE_CONSTANTS_H

#include <vulkan/vulkan.h>

#include "Util/Util.h"
#include "Externals/GLM.h"

namespace Renderer::AO::XeGTAO::Denoise
{
    struct PushConstant
    {
        u32 samplerIndex;
        u32 sourceEdgesIndex;
        u32 sourceAOIndex;
        u32 outAOIndex;
        u32 finalApply;
    };
}

#endif
