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

#ifndef DEPTH_PREFILTER_CONSTANTS_H
#define DEPTH_PREFILTER_CONSTANTS_H

#include <vulkan/vulkan.h>

#include "Util/Util.h"

namespace Renderer::AO::XeGTAO::DepthPreFilter
{
    struct PushConstant
    {
        u32 depthSamplerIndex;
        u32 sceneDepthIndex;

        u32 outDepthMip0Index;
        u32 outDepthMip1Index;
        u32 outDepthMip2Index;
        u32 outDepthMip3Index;
        u32 outDepthMip4Index;

        f32 depthLinearizeMul;
        f32 depthLinearizeAdd;

        f32 effectRadius;
        f32 effectFalloffRange;
        f32 radiusMultiplier;
    };
}

#endif
