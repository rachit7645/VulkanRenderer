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

#ifndef TILE_LIGHT_INDEX_BUFFER_H
#define TILE_LIGHT_INDEX_BUFFER_H

#include "Vulkan/Buffer.h"
#include "Util/DeletionQueue.h"

namespace Renderer::Buffers
{
    class TileLightIndexBuffer
    {
    public:
        void Update
        (
            usize tileCount,
            VkDevice device,
            VmaAllocator allocator,
            Util::DeletionQueue& deletionQueue
        );

        void Destroy(VmaAllocator allocator);

        Vk::Buffer buffer = {};
    };
}

#endif
