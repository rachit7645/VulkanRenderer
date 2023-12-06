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

namespace Vk
{
    void Pipeline::Bind(VkCommandBuffer cmdBuffer, VkPipelineBindPoint bindPoint)
    {
        // Bind pipeline
        vkCmdBindPipeline
        (
            cmdBuffer,
            bindPoint,
            handle
        );
    }

    void Pipeline::BindDescriptors
    (
        VkCommandBuffer cmdBuffer,
        VkPipelineBindPoint bindPoint,
        u32 firstSet,
        const std::span<const VkDescriptorSet> descriptors
    )
    {
        // Bind
        vkCmdBindDescriptorSets
        (
            cmdBuffer,
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
        VkCommandBuffer cmdBuffer,
        VkPipelineStageFlags stage,
        u32 offset,
        u32 size,
        void* pValues
    )
    {
        // Load
        vkCmdPushConstants
        (
            cmdBuffer,
            layout,
            stage,
            offset,
            size,
            pValues
        );
    }
}