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

#include "DrawCallBuffer.h"

#include "Util/Log.h"
#include "Vulkan/DebugUtils.h"
#include "Renderer/Buffers/MeshBuffer.h"

namespace Renderer::Buffers
{
    DrawCallBuffer::DrawCallBuffer(VkDevice device, VmaAllocator allocator)
    {
        drawCallBuffer = Vk::Buffer
        (
            allocator,
            sizeof(u32) + MAX_MESH_COUNT * sizeof(VkDrawIndexedIndirectCommand),
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        );

        instanceIndexBuffer = Vk::Buffer
        (
            allocator,
            MAX_MESH_COUNT * sizeof(u32),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            0,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
        );

        drawCallBuffer.GetDeviceAddress(device);
        instanceIndexBuffer.GetDeviceAddress(device);
    }

    void DrawCallBuffer::Destroy(VmaAllocator allocator)
    {
        drawCallBuffer.Destroy(allocator);
        instanceIndexBuffer.Destroy(allocator);
    }
}
