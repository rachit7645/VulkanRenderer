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

#ifndef OBJECTS_SAMPLERS_H
#define OBJECTS_SAMPLERS_H

#include "Vulkan/TextureManager.h"

namespace Renderer::Objects
{
    class Samplers
    {
    public:
        Samplers
        (
            const Vk::Context& context,
            Vk::MegaSet& megaSet,
            Vk::TextureManager& textureManager
        );

        void Update
        (
            const Vk::Context& context,
            const VkExtent2D& renderExtent,
            const VkExtent2D& swapchainExtent,
            Vk::MegaSet& megaSet,
            Vk::TextureManager& textureManager,
            Util::DeletionQueue& deletionQueue
        );

        Vk::SamplerID pointSamplerID           = 0;
        Vk::SamplerID linearSamplerID          = 0;
        Vk::SamplerID textureSamplerID         = 0;
        Vk::SamplerID iblSamplerID             = 0;
        Vk::SamplerID pointShadowSamplerID     = 0;
        Vk::SamplerID imguiSamplerID           = 0;
        Vk::SamplerID spotShadowSamplerID      = 0;
        Vk::SamplerID bloomDownsampleSamplerID = 0;
        Vk::SamplerID bloomUpsampleSamplerID   = 0;
    };
}

#endif
