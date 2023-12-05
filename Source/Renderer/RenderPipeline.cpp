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
#include "Util/Log.h"

namespace Renderer
{
    // Max textures
    constexpr usize MAX_TEXTURE_COUNT = (1 << 10);

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

        // Pipeline data tuple
        Vk::PipelineBuilder::PipelineData pipelineData = {};
        // Build pipeline
        pipelineData = Vk::PipelineBuilder::Create(vkContext, swapchain->renderPass)
                       .AttachShader("BasicShader.vert.spv", VK_SHADER_STAGE_VERTEX_BIT)
                       .AttachShader("BasicShader.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
                       .SetDynamicStates(DYN_STATES, SetDynamicStates)
                       .SetVertexInputState()
                       .SetIAState()
                       .SetRasterizerState(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                       .SetMSAAState()
                       .SetDepthStencilState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS, VK_FALSE, {}, {})
                       .SetBlendState()
                       .AddPushConstant(VK_SHADER_STAGE_VERTEX_BIT, 0, static_cast<u32>(sizeof(BasicShaderPushConstant)))
                       .AddDescriptor(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT,   1)
                       .AddDescriptor(1, VK_DESCRIPTOR_TYPE_SAMPLER,        VK_SHADER_STAGE_FRAGMENT_BIT, 1)
                       .AddDescriptor(2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  VK_SHADER_STAGE_FRAGMENT_BIT, MAX_TEXTURE_COUNT)
                       .Build();

        // Retrieve members
        std::tie(pipeline, pipelineLayout, descriptorData) = pipelineData;

        // Create pipeline data
        CreatePipelineData(vkContext);
        // Write descriptors for 'Shared' UBO
        WriteStaticDescriptors(vkContext->device);
    }

    void RenderPipeline::WriteImageDescriptors(VkDevice device, const std::vector<Vk::ImageView>& imageViews)
    {
        // Get descriptor data
        auto& imageData = GetImageData();
        // Calculate image view count
        usize imageViewCount = imageViews.size();
        // Calculate descriptor count
        usize descriptorCount = imageViewCount * Vk::FRAMES_IN_FLIGHT;

        // Infos
        std::vector<VkDescriptorImageInfo> imageInfos = {};
        imageInfos.reserve(descriptorCount);
        // Writes
        std::vector<VkWriteDescriptorSet> imageWrites = {};
        imageWrites.reserve(descriptorCount);

        // For each frame in flight
        for (usize FIF = 0; FIF < Vk::FRAMES_IN_FLIGHT; ++FIF)
        {
            // Loop over all images
            for (usize i = 0; i < imageViewCount; ++i)
            {
                // Get image view
                auto& currentImageView = imageViews[i];

                // Calculate descriptor index
                usize descriptorIndex = i + (imageViewDescriptorIndexOffset / Vk::FRAMES_IN_FLIGHT);
                // Get descriptor
                auto currentDescriptor = imageData.setMap[FIF][descriptorIndex];

                // Image info
                VkDescriptorImageInfo imageInfo =
                {
                    .sampler     = VK_NULL_HANDLE,
                    .imageView   = currentImageView.handle,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                };
                imageInfos.emplace_back(imageInfo);

                // Image write
                VkWriteDescriptorSet imageWrite =
                {
                    .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext            = nullptr,
                    .dstSet           = currentDescriptor,
                    .dstBinding       = imageData.binding,
                    .dstArrayElement  = 0,
                    .descriptorCount  = 1,
                    .descriptorType   = imageData.type,
                    .pImageInfo       = &imageInfos.back(),
                    .pBufferInfo      = nullptr,
                    .pTexelBufferView = nullptr
                };
                imageWrites.emplace_back(imageWrite);

                // Add to map
                imageViewMap[FIF].emplace(currentImageView, currentDescriptor);
            }
        }

        // Update current index
        imageViewDescriptorIndexOffset += descriptorCount;

        // Update
        vkUpdateDescriptorSets
        (
            device,
            static_cast<u32>(imageWrites.size()),
            imageWrites.data(),
            0,
            nullptr
        );
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
            {VK_TRUE, 2.0f},
            {VK_FALSE, VK_COMPARE_OP_ALWAYS},
            {0.0f, 0.0f},
            VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            VK_FALSE
        );
    }

    void RenderPipeline::WriteStaticDescriptors(VkDevice device)
    {
        // Get UBO sets
        auto& sharedUBOData = GetSharedUBOData();
        // Get sampler sets
        auto& samplerData = GetSamplerData();

        // Writing data
        std::array<VkDescriptorBufferInfo, Vk::FRAMES_IN_FLIGHT> sharedBufferInfos  = {};
        std::array<VkDescriptorImageInfo,  Vk::FRAMES_IN_FLIGHT> samplerInfos       = {};
        // Writes
        std::array<VkWriteDescriptorSet, 2 * Vk::FRAMES_IN_FLIGHT> descriptorWrites = {};

        // Loop
        for (usize i = 0; i < Vk::FRAMES_IN_FLIGHT; ++i)
        {
            // Descriptor buffer info
            sharedBufferInfos[i] =
            {
                .buffer = sharedUBOs[i].handle,
                .offset = 0,
                .range  = VK_WHOLE_SIZE
            };

            // Descriptor sampler info
            samplerInfos[i] =
            {
                .sampler     = textureSampler.handle,
                .imageView   = {},
                .imageLayout = {}
            };

            // Buffer Write info
            descriptorWrites[2 * i] =
            {
                .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext            = nullptr,
                .dstSet           = sharedUBOData.setMap[i][0],
                .dstBinding       = sharedUBOData.binding,
                .dstArrayElement  = 0,
                .descriptorCount  = 1,
                .descriptorType   = sharedUBOData.type,
                .pImageInfo       = nullptr,
                .pBufferInfo      = &sharedBufferInfos[i],
                .pTexelBufferView = nullptr
            };

            // Sampler write info
            descriptorWrites[2 * i + 1] =
            {
                .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext            = nullptr,
                .dstSet           = samplerData.setMap[i][0],
                .dstBinding       = samplerData.binding,
                .dstArrayElement  = 0,
                .descriptorCount  = 1,
                .descriptorType   = samplerData.type,
                .pImageInfo       = &samplerInfos[i],
                .pBufferInfo      = nullptr,
                .pTexelBufferView = nullptr
            };
        }

        // Update
        vkUpdateDescriptorSets
        (
            device,
            descriptorWrites.size(),
            descriptorWrites.data(),
            0,
            nullptr
        );
    }

    const Vk::DescriptorSetData& RenderPipeline::GetSharedUBOData() const
    {
        // Check and return
        auto& data = descriptorData[0];
        assert(data.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER && "Invalid descriptor type!");
        return data;
    }

    const Vk::DescriptorSetData& RenderPipeline::GetSamplerData() const
    {
        // Check and return
        auto& data = descriptorData[1];
        assert(data.type == VK_DESCRIPTOR_TYPE_SAMPLER && "Invalid descriptor type!");
        return data;
    }

    const Vk::DescriptorSetData& RenderPipeline::GetImageData() const
    {
        // Check and return
        auto& data = descriptorData[2];
        assert(data.type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE && "Invalid descriptor type!");
        return data;
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
        // Destroy descriptor set layouts
        for (auto&& descriptor : descriptorData)
        {
            vkDestroyDescriptorSetLayout(device, descriptor.layout, nullptr);
        }
    }
}