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

#ifndef SHADER_BINDING_TABLE_H
#define SHADER_BINDING_TABLE_H

#include "Pipeline.h"
#include "Buffer.h"
#include "CommandBufferAllocator.h"

namespace Vk
{
    class ShaderBindingTable
    {
    public:
        ShaderBindingTable
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::Context& context,
            const Vk::Pipeline& pipeline,
            u32 missCount,
            u32 hitCount,
            Util::DeletionQueue& deletionQueue
        );

        void Destroy(VmaAllocator allocator);

        VkStridedDeviceAddressRegionKHR raygenRegion = {};
        VkStridedDeviceAddressRegionKHR missRegion   = {};
        VkStridedDeviceAddressRegionKHR hitRegion    = {};
    private:
        Vk::Buffer m_buffer;
    };
}

#endif
