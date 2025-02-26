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

#ifndef LIGHTING_PUSH_CONSTANT_H
#define LIGHTING_PUSH_CONSTANT_H

#include <vulkan/vulkan.h>

#include "Vulkan/Util.h"

namespace Renderer::Lighting
{
    struct PushConstant
    {
        VkDeviceAddress scene;
        VkDeviceAddress cascades;
        VkDeviceAddress pointShadows;
        VkDeviceAddress spotShadows;

        u32 gBufferSamplerIndex;
        u32 iblSamplerIndex;
        u32 shadowSamplerIndex;

        u32 gAlbedoIndex;
        u32 gNormalIndex;
        u32 sceneDepthIndex;

        u32 irradianceIndex;
        u32 preFilterIndex;
        u32 brdfLutIndex;

        u32 shadowMapIndex;
        u32 pointShadowMapIndex;
        u32 spotShadowMapIndex;
    };
}

#endif
