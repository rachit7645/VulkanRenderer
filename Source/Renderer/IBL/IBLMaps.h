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

#ifndef IBL_MAPS_H
#define IBL_MAPS_H

#include "Vulkan/TextureManager.h"
#include "Vulkan/MegaSet.h"

namespace Renderer::IBL
{
    constexpr u32 PREFILTER_MIPMAP_LEVELS = 5;

    constexpr glm::uvec2 BRDF_LUT_SIZE   = {1024, 1024};
    constexpr VkFormat   BRDF_LUT_FORMAT = VK_FORMAT_R16G16_SFLOAT;

    struct IBLMaps
    {
        void Destroy
        (
            VkDevice device,
            VmaAllocator allocator,
            Vk::TextureManager& textureManager,
            Vk::MegaSet& megaSet,
            Util::DeletionQueue& deletionQueue
        );

        Vk::TextureID skyboxID        = 0;
        Vk::TextureID irradianceMapID = 0;
        Vk::TextureID preFilterMapID  = 0;
        Vk::TextureID brdfLutID       = 0;
    };
}

#endif
