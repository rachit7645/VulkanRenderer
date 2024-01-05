/*
 *    Copyright 2023 - 2024 Rachit Khandelwal
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
#include "Util.h"

namespace Vk
{
    Pipeline::Pipeline(VkPipeline handle, VkPipelineLayout layout, const std::vector<Vk::DescriptorSetData>& descriptorData)
        : handle(handle),
          layout(layout),
          descriptorSetData(descriptorData)
    {
    }

    void Pipeline::Bind(const Vk::CommandBuffer& cmdBuffer, VkPipelineBindPoint bindPoint) const
    {
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

    void Pipeline::Destroy(const std::shared_ptr<Vk::Context>& context)
    {
        DestroyPipelineData(context->device, context->allocator);

        vkDestroyPipeline(context->device, handle, nullptr);
        vkDestroyPipelineLayout(context->device, layout, nullptr);

        for (const auto& descriptorData : descriptorSetData)
        {
            for (const auto& descriptors : descriptorData.setMap)
            {
                Vk::CheckResult(vkFreeDescriptorSets
                (
                    context->device,
                    context->descriptorPool,
                    static_cast<u32>(descriptors.size()),
                    descriptors.data()
                ));
            }

            vkDestroyDescriptorSetLayout(context->device, descriptorData.layout, nullptr);
        }
    }

    void Pipeline::DestroyPipelineData(UNUSED VkDevice device, UNUSED VmaAllocator allocator)
    {
    }
}