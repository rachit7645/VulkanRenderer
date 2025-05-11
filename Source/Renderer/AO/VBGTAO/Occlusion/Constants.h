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

#ifndef VBGTAO_OCCLUSION_CONSTANTS_H
#define VBGTAO_OCCLUSION_CONSTANTS_H

#include <vulkan/vulkan.h>

#include "Util/Util.h"

namespace Renderer::AO::VBGTAO::Occlusion
{
    struct PushConstant
    {
        VkDeviceAddress scene;

        u32 pointSamplerIndex;
        u32 linearSamplerIndex;

        u32 hilbertLUTIndex;
        u32 gNormalIndex;

        u32 preFilterDepthIndex;

        u32 outDepthDifferencesIndex;
        u32 outNoisyAOIndex;

        u32 temporalIndex;

        f32 thickness;
    };
}

#endif
