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

#include "RenderPipeline.h"

#include "Vulkan/PipelineBuilder.h"

namespace Renderer
{
    void RenderPipeline::Create(const std::shared_ptr<Vk::Context>& vkContext, const std::shared_ptr<Vk::Swapchain>& swapchain)
    {
        // Custom functions
        auto SetDynamicStates = [swapchain] (Vk::PipelineBuilder& pipelineBuilder)
        {
            // Set viewport config
            pipelineBuilder.viewport =
            {
                .x        = 0.0f,
                .y        = 0.0f,
                .width    = static_cast<f32>(swapchain->extent.width),
                .height   = static_cast<f32>(swapchain->extent.height),
                .minDepth = 0.0f,
                .maxDepth = 1.0f
            };

            // Set scissor config
            pipelineBuilder.scissor =
            {
                .offset = {0, 0},
                .extent = swapchain->extent
            };

            // Create viewport creation info
            pipelineBuilder.viewportInfo =
            {
                .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                .pNext         = nullptr,
                .flags         = 0,
                .viewportCount = 1,
                .pViewports    = &pipelineBuilder.viewport,
                .scissorCount  = 1,
                .pScissors     = &pipelineBuilder.scissor
            };
        };

        // Dynamic states
        constexpr std::array<VkDynamicState, 2> DYN_STATES = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

        // Temporary descriptor data
        auto _descriptorData = std::vector<Vk::DescriptorSetData>();
        // Pipeline data tuple
        Vk::PipelineBuilder::PipelineData pipelineData = {};
        // Build pipeline
        pipelineData = Vk::PipelineBuilder::Create(vkContext, swapchain->renderPass)
                       .AttachShader("BasicShader.vert.spv", VK_SHADER_STAGE_VERTEX_BIT)
                       .AttachShader("BasicShader.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
                       .SetDynamicStates(DYN_STATES, SetDynamicStates)
                       .SetVertexInputState()
                       .SetIAState()
                       .SetRasterizerState(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                       .SetMSAAState()
                       .SetBlendState()
                       .AddPushConstant(VK_SHADER_STAGE_VERTEX_BIT, 0, static_cast<u32>(sizeof(BasicShaderPushConstant)))
                       .AddDescriptor(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         VK_SHADER_STAGE_VERTEX_BIT,   Vk::FRAMES_IN_FLIGHT)
                       .AddDescriptor(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, Vk::FRAMES_IN_FLIGHT)
                       .Build();

        // Retrieve members
        std::tie(pipeline, pipelineLayout, _descriptorData) = pipelineData;

        // Copy descriptor data
        std::copy
        (
            _descriptorData.begin(),
            _descriptorData.end(),
            descriptorData.begin()
        );

        // Create pipeline data
        CreatePipelineData(vkContext);
        // Write descriptors for 'Shared' UBO
        WriteSharedUBODescriptors(vkContext->device);
    }

    void RenderPipeline::WriteImageDescriptors(VkDevice device, const Vk::ImageView& imageView)
    {
        // Get sampler sets
        auto& samplerData = GetTextureSamplerData();

        // Writing data
        std::array<VkDescriptorImageInfo, Vk::FRAMES_IN_FLIGHT> samplerInfos  = {};
        std::array<VkWriteDescriptorSet,  Vk::FRAMES_IN_FLIGHT> samplerWrites = {};

        // Loop
        for (usize i = 0; i < samplerWrites.size(); ++i)
        {
            // Descriptor buffer info
            samplerInfos[i] =
            {
                .sampler     = textureSampler.handle,
                .imageView   = imageView.handle,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };

            // Write info
            samplerWrites[i] =
            {
                .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext            = nullptr,
                .dstSet           = samplerData.sets[i],
                .dstBinding       = samplerData.binding,
                .dstArrayElement  = 0,
                .descriptorCount  = 1,
                .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo       = &samplerInfos[i],
                .pBufferInfo      = nullptr,
                .pTexelBufferView = nullptr
            };
        }

        // Update
        vkUpdateDescriptorSets
        (
            device,
            samplerWrites.size(),
            samplerWrites.data(),
            0,
            nullptr
        );
    }

    Vk::DescriptorSetData& RenderPipeline::GetSharedUBOData()
    {
        // Return
        return descriptorData[0];
    }

    Vk::DescriptorSetData& RenderPipeline::GetTextureSamplerData()
    {
        // Return
        return descriptorData[1];
    }

    void RenderPipeline::CreatePipelineData(const std::shared_ptr<Vk::Context>& vkContext)
    {
        // Create shared buffers
        for (auto&& shared : sharedUBOs)
        {
            // Create buffer
            shared = Vk::Buffer
            (
                vkContext,
                static_cast<u32>(sizeof(SharedBuffer)),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            );
            // Map
            shared.Map(vkContext->device);
        }

        // Create texture sampler
        textureSampler = Vk::Sampler
        (
            vkContext->device,
            {VK_FILTER_LINEAR, VK_FILTER_LINEAR},
            VK_SAMPLER_MIPMAP_MODE_LINEAR,
            {VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT},
            0.0f,
            {VK_FALSE, 0.0f},
            {VK_FALSE, VK_COMPARE_OP_ALWAYS},
            {0.0f, 0.0f},
            VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            VK_FALSE
        );
    }

    void RenderPipeline::WriteSharedUBODescriptors(VkDevice device)
    {
        // Get ubo sets
        auto& sharedUBOData = GetSharedUBOData();

        // Writing data
        std::array<VkDescriptorBufferInfo, Vk::FRAMES_IN_FLIGHT> sharedBufferInfos  = {};
        std::array<VkWriteDescriptorSet,   Vk::FRAMES_IN_FLIGHT> sharedBufferWrites = {};

        // Loop
        for (usize i = 0; i < sharedBufferWrites.size(); ++i)
        {
            // Descriptor buffer info
            sharedBufferInfos[i] =
            {
                .buffer = sharedUBOs[i].handle,
                .offset = 0,
                .range  = VK_WHOLE_SIZE
            };

            // Write info
            sharedBufferWrites[i] =
            {
                .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext            = nullptr,
                .dstSet           = sharedUBOData.sets[i],
                .dstBinding       = sharedUBOData.binding,
                .dstArrayElement  = 0,
                .descriptorCount  = 1,
                .descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pImageInfo       = nullptr,
                .pBufferInfo      = &sharedBufferInfos[i],
                .pTexelBufferView = nullptr
            };
        }

        // Update
        vkUpdateDescriptorSets
        (
            device,
            sharedBufferWrites.size(),
            sharedBufferWrites.data(),
            0,
            nullptr
        );
    }

    void RenderPipeline::Destroy(VkDevice device)
    {
        // Destroy UBOs
        for (auto&& shared : sharedUBOs) shared.DeleteBuffer(device);
        // Destroy sampler
        textureSampler.Destroy(device);
        // Destroy pipeline
        vkDestroyPipeline(device, pipeline, nullptr);
        // Destroy pipeline layout
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        // Destroy descriptor set layout
        for (auto&& descriptor : descriptorData) vkDestroyDescriptorSetLayout(device, descriptor.layout, nullptr);
    }
}