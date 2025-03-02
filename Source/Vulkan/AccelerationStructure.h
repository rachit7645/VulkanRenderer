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

#ifndef ACCELERATION_STRUCTURE_H
#define ACCELERATION_STRUCTURE_H

#include "Constants.h"
#include "CommandBuffer.h"
#include "Context.h"
#include "Buffer.h"
#include "Models/ModelManager.h"
#include "Renderer/RenderObject.h"

namespace Vk
{
    class AccelerationStructure
    {
    public:
        struct AS
        {
            VkAccelerationStructureKHR as;
            Vk::Buffer                 buffer;
            VkDeviceAddress            deviceAddress;
        };

        void BuildBottomLevelAS
        (
            const Vk::Context& context,
            const Models::ModelManager& modelManager,
            const std::span<const Renderer::RenderObject> renderObjects
        );

        void BuildTopLevelAS
        (
            usize FIF,
            const Vk::CommandBuffer& cmdBuffer,
            VkDevice device,
            VmaAllocator allocator,
            const std::span<const Renderer::RenderObject> renderObjects
        );

        void Destroy(VkDevice device, VmaAllocator allocator);

        std::vector<AS>                      bottomLevelASes;
        std::array<AS, Vk::FRAMES_IN_FLIGHT> topLevelASes;
    private:
        std::array<Vk::Buffer, Vk::FRAMES_IN_FLIGHT> m_instanceBuffers;
        std::array<Vk::Buffer, Vk::FRAMES_IN_FLIGHT> m_scratchBuffers;

        std::array<Util::DeletionQueue, Vk::FRAMES_IN_FLIGHT> m_deletionQueues;
    };
}

#endif
