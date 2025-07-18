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

#ifndef SHADOW_RT_PIPELINE_H
#define SHADOW_RT_PIPELINE_H

#include "Vulkan/Pipeline.h"
#include "Vulkan/TextureManager.h"
#include "Vulkan/MegaSet.h"

namespace Renderer::ShadowRT
{
    class Pipeline : public Vk::Pipeline
    {
    public:
        Pipeline
        (
            const Vk::Context& context,
            Vk::MegaSet& megaSet,
            Vk::TextureManager& textureManager
        );

        Vk::SamplerID gBufferSamplerID = 0;
        Vk::SamplerID textureSamplerID = 0;
    };
}

#endif
