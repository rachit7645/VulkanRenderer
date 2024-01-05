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

#include "ForwardPipeline.h"
#include "Models/Vertex.h"
#include "Vulkan/Builders/PipelineBuilder.h"

namespace Renderer::Pipelines
{
    // Usings
    using Models::Vertex;

    ForwardPipeline::ForwardPipeline
    (
        const std::shared_ptr<Vk::Context>& context,
        VkFormat colorFormat,
        VkFormat depthFormat,
        VkExtent2D extent
    )
        : Vk::Pipeline(CreatePipeline(context, colorFormat, depthFormat, extent))
    {
        CreatePipelineData(context);
        WriteStaticDescriptors(context->device);
    }

    void ForwardPipeline::WriteMaterialDescriptors(VkDevice device, const std::span<const Models::Material> materials)
    {
        auto& materialData = GetMaterialData();

        usize materialCount   = materials.size();
        usize descriptorCount = materialCount * Vk::FRAMES_IN_FLIGHT;
        usize writeCount      = descriptorCount * Models::Material::MATERIAL_COUNT;

        // Pre-allocate
        std::vector<VkDescriptorImageInfo> imageInfos = {};
        imageInfos.resize(writeCount);
        std::vector<VkWriteDescriptorSet> imageWrites = {};
        imageWrites.resize(writeCount);

        for (usize FIF = 0; FIF < Vk::FRAMES_IN_FLIGHT; ++FIF)
        {
            for (usize i = 0; i < materialCount; ++i)
            {
                usize descriptorIndex = i + (textureDescriptorIndexOffset / Vk::FRAMES_IN_FLIGHT);

                VkDescriptorSet currentDescriptor = materialData.setMap[FIF][descriptorIndex];
                const auto&     currentViews      = materials[i].GetViews();

                for (usize j = 0; j < currentViews.size(); ++j)
                {
                    auto& currentImageView = currentViews[j];
                    usize writeIndex       = FIF * materialCount * currentViews.size() + i * currentViews.size() + j;

                    imageInfos[writeIndex] =
                    {
                        .sampler     = VK_NULL_HANDLE,
                        .imageView   = currentImageView.handle,
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                    };

                    imageWrites[writeIndex] =
                    {
                        .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                        .pNext            = nullptr,
                        .dstSet           = currentDescriptor,
                        .dstBinding       = materialData.binding,
                        .dstArrayElement  = static_cast<u32>(j),
                        .descriptorCount  = 1,
                        .descriptorType   = materialData.type,
                        .pImageInfo       = &imageInfos[writeIndex],
                        .pBufferInfo      = nullptr,
                        .pTexelBufferView = nullptr
                    };
                }

                materialMap[FIF].emplace(materials[i], currentDescriptor);
            }
        }

        textureDescriptorIndexOffset += descriptorCount;

        vkUpdateDescriptorSets
        (
            device,
            static_cast<u32>(imageWrites.size()),
            imageWrites.data(),
            0,
            nullptr
        );
    }

    const Vk::DescriptorSetData& ForwardPipeline::GetSceneUBOData() const
    {
        return descriptorSetData[0];
    }

    const Vk::DescriptorSetData& ForwardPipeline::GetSamplerData() const
    {
        return descriptorSetData[1];
    }

    const Vk::DescriptorSetData& ForwardPipeline::GetMaterialData() const
    {
        return descriptorSetData[2];
    }

    Vk::Pipeline ForwardPipeline::CreatePipeline
    (
        const std::shared_ptr<Vk::Context>& context,
        VkFormat colorFormat,
        VkFormat depthFormat,
        VkExtent2D extent
    )
    {
        auto SetDynamicStates = [&extent] (Vk::Builders::PipelineBuilder& pipelineBuilder)
        {
            pipelineBuilder.viewport =
            {
                .x        = 0.0f,
                .y        = 0.0f,
                .width    = static_cast<f32>(extent.width),
                .height   = static_cast<f32>(extent.height),
                .minDepth = 0.0f,
                .maxDepth = 1.0f
            };

            pipelineBuilder.scissor =
            {
                .offset = {0, 0},
                .extent = extent
            };

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

        constexpr std::array<VkDynamicState, 2> DYN_STATES         = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        constexpr usize                         MAX_MATERIAL_COUNT = (1 << 10) / Models::Material::MATERIAL_COUNT;

        auto colorFormats = std::span(&colorFormat, 1);

        return Vk::Builders::PipelineBuilder(context)
              .SetRenderingInfo(colorFormats, depthFormat, VK_FORMAT_UNDEFINED)
              .AttachShader("Forward.vert.spv", VK_SHADER_STAGE_VERTEX_BIT)
              .AttachShader("Forward.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
              .SetDynamicStates(DYN_STATES, SetDynamicStates)
              .SetVertexInputState(Vertex::GetBindingDescription(), Vertex::GetVertexAttribDescription())
              .SetIAState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE)
              .SetRasterizerState(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL)
              .SetMSAAState()
              .SetDepthStencilState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS, VK_FALSE, {}, {})
              .SetBlendState()
              .AddPushConstant(VK_SHADER_STAGE_VERTEX_BIT, 0, static_cast<u32>(sizeof(VSPushConstant)))
              .AddDescriptor(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1, 1)
              .AddDescriptor(1, VK_DESCRIPTOR_TYPE_SAMPLER,        VK_SHADER_STAGE_FRAGMENT_BIT, 1, 1)
              .AddDescriptor(2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  VK_SHADER_STAGE_FRAGMENT_BIT, Models::Material::MATERIAL_COUNT, MAX_MATERIAL_COUNT)
              .Build();
    }

    void ForwardPipeline::CreatePipelineData(const std::shared_ptr<Vk::Context>& context)
    {
        for (auto&& sceneUBO : sceneUBOs)
        {
            sceneUBO = Vk::Buffer
            (
                context->allocator,
                static_cast<u32>(sizeof(SceneBuffer)),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                VMA_MEMORY_USAGE_AUTO
            );
        }

        constexpr auto SAMPLER_ANISOTROPY = 4.0f;
        auto anisotropy = std::min(SAMPLER_ANISOTROPY, context->physicalDeviceLimits.maxSamplerAnisotropy);

        textureSampler = Vk::Sampler
        (
            context->device,
            {VK_FILTER_LINEAR, VK_FILTER_LINEAR},
            VK_SAMPLER_MIPMAP_MODE_LINEAR,
            {VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT},
            0.0f,
            {VK_TRUE, anisotropy},
            {VK_FALSE, VK_COMPARE_OP_ALWAYS},
            {0.0f, VK_LOD_CLAMP_NONE},
            VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            VK_FALSE
        );
    }

    void ForwardPipeline::DestroyPipelineData(VkDevice device, VmaAllocator allocator)
    {
        for (auto&& sceneUBO : sceneUBOs)
        {
            sceneUBO.Destroy(allocator);
        }

        textureSampler.Destroy(device);
    }

    void ForwardPipeline::WriteStaticDescriptors(VkDevice device)
    {
        auto& sharedUBOData = GetSceneUBOData();
        auto& samplerData   = GetSamplerData();

        std::array<VkDescriptorBufferInfo,   Vk::FRAMES_IN_FLIGHT> sharedBufferInfos  = {};
        std::array<VkDescriptorImageInfo,    Vk::FRAMES_IN_FLIGHT> samplerInfos       = {};
        std::array<VkWriteDescriptorSet, 2 * Vk::FRAMES_IN_FLIGHT> descriptorWrites   = {};

        for (usize i = 0; i < Vk::FRAMES_IN_FLIGHT; ++i)
        {
            sharedBufferInfos[i] =
            {
                .buffer = sceneUBOs[i].handle,
                .offset = 0,
                .range  = VK_WHOLE_SIZE
            };

            samplerInfos[i] =
            {
                .sampler     = textureSampler.handle,
                .imageView   = {},
                .imageLayout = {}
            };

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

        vkUpdateDescriptorSets
        (
            device,
            descriptorWrites.size(),
            descriptorWrites.data(),
            0,
            nullptr
        );
    }
}