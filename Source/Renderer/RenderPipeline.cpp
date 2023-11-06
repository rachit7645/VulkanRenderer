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
    void RenderPipeline::Create(const std::shared_ptr<Vk::Context>& vkContext)
    {
        // Custom functions
        auto SetDynamicStates = [vkContext](Vk::PipelineBuilder& pipelineBuilder)
        {
            // Set viewport config
            pipelineBuilder.viewport =
            {
                .x        = 0.0f,
                .y        = 0.0f,
                .width    = static_cast<f32>(vkContext->swapChainExtent.width),
                .height   = static_cast<f32>(vkContext->swapChainExtent.height),
                .minDepth = 0.0f,
                .maxDepth = 1.0f
            };

            // Set scissor config
            pipelineBuilder.scissor =
            {
                .offset = {0, 0},
                .extent = vkContext->swapChainExtent
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
        constexpr std::array<VkDynamicState, 2> dynStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        // Pipeline data tuple
        std::tuple<VkPipeline, VkPipelineLayout, VkDescriptorSetLayout> pipelineData = {};

        // Build pipeline
        pipelineData = Vk::PipelineBuilder::Create(vkContext->device, vkContext->renderPass)
                       .AttachShader("BasicShader.vert.spv", VK_SHADER_STAGE_VERTEX_BIT)
                       .AttachShader("BasicShader.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
                       .SetDynamicStates(dynStates, SetDynamicStates)
                       .SetVertexInputState()
                       .SetIAState()
                       .SetRasterizerState(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                       .SetMSAAState()
                       .SetBlendState()
                       .AddPushConstant(VK_SHADER_STAGE_VERTEX_BIT, 0, static_cast<u32>(sizeof(BasicShaderPushConstant)))
                       .AddDescriptor(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
                       .Build();

        // Retrieve members
        std::tie(pipeline, pipelineLayout, descriptorLayout) = pipelineData;

        // Create shared buffers
        for (auto&& shared : sharedUBOs)
        {
            // Create buffer
            shared = Vk::Buffer
            (
                vkContext->device,
                static_cast<u32>(sizeof(SharedBuffer)),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                vkContext->phyMemProperties
            );

            // Map
            shared.Map(vkContext->device);
        }

        // Allocate sets
        auto _sharedUBOSets = vkContext->AllocateDescriptorSets(sharedUBOSets.size(), descriptorLayout);
        // Convert
        std::move
        (
            _sharedUBOSets.begin(),
            _sharedUBOSets.begin() + static_cast<ssize>(sharedUBOSets.size()),
            sharedUBOSets.begin()
        );

        // Loop
        for (usize i = 0; i < Vk::FRAMES_IN_FLIGHT; ++i)
        {
            // Descriptor buffer info
            VkDescriptorBufferInfo bufferInfo =
            {
                .buffer = sharedUBOs[i].handle,
                .offset = 0,
                .range  = VK_WHOLE_SIZE
            };

            // Write info
            VkWriteDescriptorSet descriptorWrite =
            {
                .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext            = nullptr,
                .dstSet           = sharedUBOSets[i],
                .dstBinding       = 0,
                .dstArrayElement  = 0,
                .descriptorCount  = 1,
                .descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pImageInfo       = nullptr,
                .pBufferInfo      = &bufferInfo,
                .pTexelBufferView = nullptr
            };

            // Update
            vkUpdateDescriptorSets
            (
                vkContext->device,
                1,
                &descriptorWrite,
                0,
                nullptr
            );
        }
    }

    void RenderPipeline::Destroy(const std::shared_ptr<Vk::Context>& vkContext)
    {
        // Destroy UBOs
        for (auto&& shared : sharedUBOs) shared.DeleteBuffer(vkContext->device);
        // Destroy pipeline
        vkDestroyPipeline(vkContext->device, pipeline, nullptr);
        // Destroy pipeline layout
        vkDestroyPipelineLayout(vkContext->device, pipelineLayout, nullptr);
        // Destroy descriptor set layout
        vkDestroyDescriptorSetLayout(vkContext->device, descriptorLayout, nullptr);
    }
}