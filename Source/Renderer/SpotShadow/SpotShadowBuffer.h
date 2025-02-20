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

#ifndef SPOT_SHADOW_BUFFER_H
#define SPOT_SHADOW_BUFFER_H

#include "Externals/GLM.h"
#include "Vulkan/Buffer.h"
#include "Vulkan/Constants.h"

namespace Renderer::SpotShadow
{
    class SpotShadowBuffer
    {
    public:
        SpotShadowBuffer(VkDevice device, VmaAllocator allocator);

        void LoadMatrices
        (
            usize FIF,
            VmaAllocator allocator,
            const std::span<const glm::mat4> matrices
        );

        void Destroy(VmaAllocator allocator);

        std::array<Vk::Buffer, Vk::FRAMES_IN_FLIGHT> buffers;
    };
}

#endif
