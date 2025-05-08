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

#ifndef IBL_MAPS_H
#define IBL_MAPS_H

#include "Vulkan/GeometryBuffer.h"
#include "Vulkan/TextureManager.h"
#include "Vulkan/MegaSet.h"

namespace Renderer::IBL
{
    struct IBLMaps
    {
        void Destroy
        (
            const Vk::Context& context,
            Vk::TextureManager& textureManager,
            Vk::MegaSet& megaSet,
            Util::DeletionQueue& deletionQueue
        ) const;

        u32 skyboxID        = 0;
        u32 irradianceMapID = 0;
        u32 preFilterMapID  = 0;
        u32 brdfLutID       = 0;
    };
}

#endif
