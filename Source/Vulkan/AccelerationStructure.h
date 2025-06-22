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
#include "GraphicsTimeline.h"
#include "Models/ModelManager.h"
#include "Renderer/RenderObject.h"

namespace Vk
{
    class AccelerationStructure
    {
    public:
        struct AS
        {
            VkAccelerationStructureKHR handle        = VK_NULL_HANDLE;
            Vk::Buffer                 buffer        = {};
            VkDeviceAddress            deviceAddress = 0;
        };

        void BuildBottomLevelAS
        (
            usize frameIndex,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::Context& context,
            const Models::ModelManager& modelManager,
            const std::span<const Renderer::RenderObject> renderObjects,
            Util::DeletionQueue& deletionQueue
        );

        void TryCompactBottomLevelAS
        (
            const Vk::CommandBuffer& cmdBuffer,
            VkDevice device,
            VmaAllocator allocator,
            const Vk::GraphicsTimeline& timeline,
            Util::DeletionQueue& deletionQueue
        );

        void BuildTopLevelAS
        (
            usize FIF,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::Context& context,
            const Models::ModelManager& modelManager,
            const std::span<const Renderer::RenderObject> renderObjects,
            Util::DeletionQueue& deletionQueue
        );

        void Destroy(VkDevice device, VmaAllocator allocator);

        std::array<AS, Vk::FRAMES_IN_FLIGHT> topLevelASes = {};
    private:
        std::vector<AS> m_bottomLevelASes;

        std::array<Vk::Buffer, Vk::FRAMES_IN_FLIGHT> m_instanceBuffers = {};
        std::array<Vk::Buffer, Vk::FRAMES_IN_FLIGHT> m_scratchBuffers  = {};

        VkQueryPool m_compactionQueryPool        = VK_NULL_HANDLE;
        usize       m_initialBLASBuildFrameIndex = std::numeric_limits<usize>::max();
    };
}

#endif
