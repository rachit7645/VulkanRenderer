/*
 *    Copyright 2023 Rachit Khandelwal
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "Pipeline.h"
#include "CommandBuffer.h"

namespace Vk
{
    void Pipeline::Bind(const Vk::CommandBuffer& cmdBuffer, VkPipelineBindPoint bindPoint) const
    {
        // Bind pipeline
        vkCmdBindPipeline
        (
            cmdBuffer.handle,
            bindPoint,
            handle
        );
    }

    void Pipeline::BindDescriptors
    (
        const Vk::CommandBuffer& cmdBuffer,
        VkPipelineBindPoint bindPoint,
        u32 firstSet,
        const std::span<const VkDescriptorSet> descriptors
    ) const
    {
        // Bind
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

    void Pipeline::LoadPushConstants
    (
        const Vk::CommandBuffer& cmdBuffer,
        VkPipelineStageFlags stage,
        u32 offset,
        u32 size,
        void* pValues
    ) const
    {
        // Load
        vkCmdPushConstants
        (
            cmdBuffer.handle,
            layout,
            stage,
            offset,
            size,
            pValues
        );
    }

    void Pipeline::Destroy(VkDevice device) const
    {
        // Destroy pipeline
        vkDestroyPipeline(device, handle, nullptr);
        // Destroy pipeline layout
        vkDestroyPipelineLayout(device, layout, nullptr);
        // Destroy descriptor set layouts
        for (auto&& descriptor : descriptorSetData)
        {
            vkDestroyDescriptorSetLayout(device, descriptor.layout, nullptr);
        }
    }
}