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

#ifndef PIPELINE_H
#define PIPELINE_H

#include <vector>
#include <vulkan/vulkan.h>

#include "Context.h"
#include "Util/Util.h"
#include "CommandBuffer.h"
#include "DescriptorSet.h"

namespace Vk
{
    class Pipeline
    {
    public:
        Pipeline() = default;
        virtual ~Pipeline() = default;

        // No copying
        Pipeline(const Pipeline&) = delete;
        Pipeline& operator=(const Pipeline&) = delete;

        // Only moving
        Pipeline(Pipeline&& other) noexcept = default;
        Pipeline& operator=(Pipeline&& other) noexcept = default;

        void Bind(const Vk::CommandBuffer& cmdBuffer, VkPipelineBindPoint bindPoint) const;

        void BindDescriptors
        (
            const Vk::CommandBuffer& cmdBuffer,
            VkPipelineBindPoint bindPoint,
            u32 firstSet,
            const std::span<const VkDescriptorSet> descriptors
        ) const;

        void LoadPushConstants
        (
            const Vk::CommandBuffer& cmdBuffer,
            VkShaderStageFlags stages,
            u32 offset,
            u32 size,
            void* pValues
        ) const;

        void Destroy(VkDevice device);

        // Handles
        VkPipeline       handle = {};
        VkPipelineLayout layout = {};

        // Deletion queue
        Util::DeletionQueue m_deletionQueue = {};
    };
}

#endif
