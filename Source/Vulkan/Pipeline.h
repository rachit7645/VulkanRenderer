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

#include <vulkan/vulkan.h>

#include "Context.h"
#include "Util/Types.h"
#include "CommandBuffer.h"

namespace Vk
{
    class Pipeline
    {
    public:
        void Bind(const Vk::CommandBuffer& cmdBuffer) const;

        void BindDescriptors
        (
            const Vk::CommandBuffer& cmdBuffer,
            u32 firstSet,
            const std::span<const VkDescriptorSet> descriptors
        ) const;

        void PushConstants
        (
            const Vk::CommandBuffer& cmdBuffer,
            VkShaderStageFlags stages,
            u32 offset,
            u32 size,
            const void* pValues
        ) const;

        template<typename T>
        void PushConstants
        (
            const Vk::CommandBuffer& cmdBuffer,
            VkShaderStageFlags stages,
            u32 offset,
            const T& value
        ) const
        {
            PushConstants
            (
                cmdBuffer,
                stages,
                offset,
                sizeof(T),
                &value
            );
        }

        template<typename T>
        void PushConstants
        (
            const Vk::CommandBuffer& cmdBuffer,
            VkShaderStageFlags stages,
            const T& value
        ) const
        {
            PushConstants<T>
            (
                cmdBuffer,
                stages,
                0,
                value
            );
        }

        void Destroy(VkDevice device) const;

        VkPipeline          handle    = VK_NULL_HANDLE;
        VkPipelineLayout    layout    = VK_NULL_HANDLE;
        VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    };
}

#endif
