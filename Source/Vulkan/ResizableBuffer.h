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

#ifndef RESIZABLE_BUFFER_H
#define RESIZABLE_BUFFER_H

#include "Buffer.h"
#include "Util/DeletionQueue.h"

namespace Vk
{
    class ResizableBuffer
    {
    public:
        void Reserve
        (
            VkDevice device,
            VmaAllocator allocator,
            const Vk::CommandBuffer& cmdBuffer,
            VkDeviceSize capacity,
            Util::DeletionQueue& deletionQueue
        );

        void Destroy(VmaAllocator allocator);

        Vk::Buffer buffer = {};
    };
}

#endif