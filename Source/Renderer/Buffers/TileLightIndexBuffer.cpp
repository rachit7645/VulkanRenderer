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

#include "TileLightIndexBuffer.h"

#include "TiledLighting/Common.h"
#include "Vulkan/DebugUtils.h"

namespace Renderer::Buffers
{
    void TileLightIndexBuffer::Update
    (
        usize tileCount,
        VkDevice device,
        VmaAllocator allocator,
        Util::DeletionQueue& deletionQueue
    )
    {
        const VkDeviceSize requiredSize = tileCount * sizeof(TiledLighting::TileLightIndices);

        if (buffer.size >= requiredSize)
        {
            return;
        }

        deletionQueue.Push([allocator, _buffer = buffer] mutable
        {
           _buffer.Destroy(allocator);
        });

        buffer = Vk::Buffer
        (
            device,
            allocator,
            requiredSize,
            0,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        );

        Vk::SetDebugName(device, buffer.handle, "TileLightIndexBuffer");
    }

    void TileLightIndexBuffer::Destroy(VmaAllocator allocator)
    {
        buffer.Destroy(allocator);
    }
}
