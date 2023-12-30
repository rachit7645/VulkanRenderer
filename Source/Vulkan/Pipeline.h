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

#ifndef PIPELINE_H
#define PIPELINE_H

#include <vector>
#include <vulkan/vulkan.h>

#include "DescriptorSetData.h"
#include "RenderPass.h"
#include "Context.h"
#include "Util/Util.h"
#include "CommandBuffer.h"

namespace Vk
{
    class Pipeline
    {
    public:
        // Default constructor
        Pipeline() = default;
        // Constructor
        Pipeline(VkPipeline handle, VkPipelineLayout layout, const std::vector<Vk::DescriptorSetData>& descriptorData);
        // Virtual destructor
        virtual ~Pipeline() = default;

        // Bind pipeline to command buffer
        void Bind(const Vk::CommandBuffer& cmdBuffer, VkPipelineBindPoint bindPoint) const;

        // Bind descriptor sets
        void BindDescriptors
        (
            const Vk::CommandBuffer& cmdBuffer,
            VkPipelineBindPoint bindPoint,
            u32 firstSet,
            const std::span<const VkDescriptorSet> descriptors
        ) const;

        // Load push constants
        void LoadPushConstants
        (
            const Vk::CommandBuffer& cmdBuffer,
            VkPipelineStageFlags stage,
            u32 offset,
            u32 size,
            void* pValues
        ) const;

        // Destroy
        void Destroy(VkDevice device, VmaAllocator allocator);

        // Pipeline handle
        VkPipeline handle = {};
        // Pipeline layout
        VkPipelineLayout layout = {};
        // Descriptor data
        std::vector<Vk::DescriptorSetData> descriptorSetData = {};
    private:
        // Destroy per-pipeline data
        virtual void DestroyPipelineData(VkDevice device, VmaAllocator allocator);
    };
}

#endif
