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

#include "Pipeline.h"

#include <volk/volk.h>

#include "CommandBuffer.h"
#include "Util.h"

namespace Vk
{
    Pipeline::Pipeline(VkPipeline pipeline, VkPipelineLayout layout, VkPipelineBindPoint bindPoint)
        : handle(pipeline),
          layout(layout),
          bindPoint(bindPoint)
    {
    }

    void Pipeline::Bind(const Vk::CommandBuffer& cmdBuffer) const
    {
        vkCmdBindPipeline(cmdBuffer.handle, bindPoint, handle);
    }

    void Pipeline::BindDescriptors
    (
        const Vk::CommandBuffer& cmdBuffer,
        u32 firstSet,
        const std::span<const VkDescriptorSet> descriptors
    ) const
    {
        vkCmdBindDescriptorSets
        (
            cmdBuffer.handle,
            bindPoint,
            layout,
            firstSet,
            static_cast<u32>(descriptors.size()),
            descriptors.data(),
            0,
            nullptr
        );
    }

    void Pipeline::PushConstants
    (
        const Vk::CommandBuffer& cmdBuffer,
        VkShaderStageFlags stages,
        u32 offset,
        u32 size,
        const void* pValues
    ) const
    {
        vkCmdPushConstants
        (
            cmdBuffer.handle,
            layout,
            stages,
            offset,
            size,
            pValues
        );
    }

    void Pipeline::Destroy(VkDevice device) const
    {
        vkDestroyPipeline(device, handle, nullptr);
        vkDestroyPipelineLayout(device, layout, nullptr);
    }
}