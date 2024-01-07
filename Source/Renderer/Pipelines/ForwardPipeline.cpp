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
#include "Vulkan/DescriptorWriter.h"
#include "Vulkan/Builders/DescriptorLayoutBuilder.h"

namespace Renderer::Pipelines
{
    // Usings
    using Models::Vertex;

    // Constants
    constexpr auto STATIC_LAYOUT_ID   = "FORWARD_PIPELINE_STATIC_LAYOUT";
    constexpr auto STATIC_SET_ID      = "FORWARD_PIPELINE_STATIC_SETS";
    constexpr auto MATERIAL_LAYOUT_ID = "FORWARD_PIPELINE_MATERIAL_LAYOUT";
    constexpr auto MATERIAL_SET_ID    = "FORWARD_PIPELINE_MATERIAL_SETS";

    ForwardPipeline::ForwardPipeline
    (
        const std::shared_ptr<Vk::Context>& context,
        VkFormat colorFormat,
        VkFormat depthFormat
    )
        : Vk::Pipeline(CreatePipeline(context, colorFormat, depthFormat))
    {
        CreatePipelineData(context);
        WriteStaticDescriptors(context->device, context->descriptorCache);
    }

    void ForwardPipeline::WriteMaterialDescriptors
    (
        VkDevice device,
        Vk::DescriptorCache& descriptorCache,
        const std::span<const Models::Material> materials
    )
    {
        const usize materialCount = materials.size();

        Vk::DescriptorWriter writer = {};

        for (usize i = 0; i < materialCount; ++i)
        {
            const auto& currentSets  = descriptorCache.AllocateSets
            (
                MATERIAL_SET_ID + std::to_string(i + materialDescriptorIDOffset),
                MATERIAL_LAYOUT_ID,
                device
            );

            const auto& currentViews = materials[i].GetViews();

            for (usize j = 0; j < currentViews.size(); ++j)
            {
                for (usize FIF = 0; FIF < Vk::FRAMES_IN_FLIGHT; ++FIF)
                {
                    writer.WriteImage
                    (
                        currentSets[FIF].handle,
                        0,
                        static_cast<u32>(j),
                        VK_NULL_HANDLE,
                        currentViews[j].handle,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
                    );
                }
            }

            materialMap.emplace(materials[i], currentSets);
        }

        materialDescriptorIDOffset += materialCount;

        writer.Update(device);
    }

    const std::array<Vk::DescriptorSet, Vk::FRAMES_IN_FLIGHT>& ForwardPipeline::GetStaticSets(Vk::DescriptorCache& descriptorCache) const
    {
        return descriptorCache.GetSets(STATIC_SET_ID);
    }

    Vk::Pipeline ForwardPipeline::CreatePipeline
    (
        const std::shared_ptr<Vk::Context>& context,
        VkFormat colorFormat,
        VkFormat depthFormat
    )
    {
        constexpr std::array<VkDynamicState, 2> DYNAMIC_STATES = {VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT, VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT};

        auto colorFormats = std::span(&colorFormat, 1);

        auto staticLayout = context->descriptorCache.AddLayout
        (
            STATIC_LAYOUT_ID,
            context->device,
            Vk::Builders::DescriptorLayoutBuilder()
                .AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                .AddBinding(1, VK_DESCRIPTOR_TYPE_SAMPLER,        1, VK_SHADER_STAGE_FRAGMENT_BIT)
                .Build(context->device)
        );

        auto materialLayout = context->descriptorCache.AddLayout
        (
            MATERIAL_LAYOUT_ID,
            context->device,
            Vk::Builders::DescriptorLayoutBuilder()
                .AddBinding(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, Models::Material::MATERIAL_COUNT, VK_SHADER_STAGE_FRAGMENT_BIT)
                .Build(context->device)
        );

        auto pipeline = Vk::Builders::PipelineBuilder(context)
            .SetRenderingInfo(colorFormats, depthFormat, VK_FORMAT_UNDEFINED)
            .AttachShader("Forward.vert.spv", VK_SHADER_STAGE_VERTEX_BIT)
            .AttachShader("Forward.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
            .SetDynamicStates(DYNAMIC_STATES)
            .SetVertexInputState(Vertex::GetBindingDescription(), Vertex::GetVertexAttribDescription())
            .SetIAState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE)
            .SetRasterizerState(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL)
            .SetMSAAState()
            .SetDepthStencilState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS, VK_FALSE, {}, {})
            .SetBlendState()
            .AddPushConstant(VK_SHADER_STAGE_VERTEX_BIT, 0, static_cast<u32>(sizeof(VSPushConstant)))
            .AddDescriptorLayout(staticLayout)
            .AddDescriptorLayout(materialLayout)
            .Build();

        context->descriptorCache.AllocateSets(STATIC_SET_ID, STATIC_LAYOUT_ID, context->device);

        return pipeline;
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

        m_deletionQueue.PushDeletor([buffers = sceneUBOs, sampler = textureSampler, context] ()
        {
            for (auto&& buffer : buffers)
            {
                buffer.Destroy(context->allocator);
            }

            sampler.Destroy(context->device);
        });
    }

    void ForwardPipeline::WriteStaticDescriptors(VkDevice device, Vk::DescriptorCache& cache)
    {
        const auto& staticSets = GetStaticSets(cache);

        Vk::DescriptorWriter writer = {};

        for (usize i = 0; i < Vk::FRAMES_IN_FLIGHT; ++i)
        {
            writer.WriteBuffer
            (
                staticSets[i].handle,
                0,
                0,
                sceneUBOs[i].handle,
                VK_WHOLE_SIZE,
                0,
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
            );

            writer.WriteImage
            (
                staticSets[i].handle,
                1,
                0,
                textureSampler.handle,
                VK_NULL_HANDLE,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_DESCRIPTOR_TYPE_SAMPLER
            );
        }

        writer.Update(device);
    }
}