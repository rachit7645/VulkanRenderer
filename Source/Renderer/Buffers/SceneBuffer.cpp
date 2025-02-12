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

#include "SceneBuffer.h"

#include "Vulkan/DebugUtils.h"

namespace Renderer::Buffers
{
    SceneBuffer::SceneBuffer(VkDevice device, VmaAllocator allocator)
    {
        for (usize i = 0; i < buffers.size(); ++i)
        {
            buffers[i] = Vk::Buffer
            (
                allocator,
                static_cast<u32>(sizeof(Renderer::Scene)),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            buffers[i].GetDeviceAddress(device);

            Vk::SetDebugName(device, buffers[i].handle, fmt::format("SceneBuffer/{}", i));
        }
    }

    void SceneBuffer::WriteScene(usize FIF, const Renderer::Scene& scene)
    {
        std::memcpy
        (
            buffers[FIF].allocInfo.pMappedData,
            &scene,
            sizeof(Renderer::Scene)
        );
    }

    void SceneBuffer::Destroy(VmaAllocator allocator)
    {
        for (auto& buffer : buffers)
        {
            buffer.Destroy(allocator);
        }
    }
}
