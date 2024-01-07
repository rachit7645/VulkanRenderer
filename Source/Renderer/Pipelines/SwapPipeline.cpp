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

#include "SwapPipeline.h"

#include "Vulkan/Builders/PipelineBuilder.h"
#include "Util/Log.h"
#include "Vulkan/DescriptorWriter.h"
#include "Vulkan/Builders/DescriptorLayoutBuilder.h"

namespace Renderer::Pipelines
{
    constexpr auto COLOR_LAYOUT_ID = "SWAPCHAIN_PIPELINE_COLOR_LAYOUT";
    constexpr auto IMAGE_SET_ID    = "SWAPCHAIN_PIPELINE_IMAGE_SETS";

    SwapPipeline::SwapPipeline(const std::shared_ptr<Vk::Context>& context, VkFormat colorFormat)
        : Vk::Pipeline(CreatePipeline(context, colorFormat))
    {
        CreatePipelineData(context);
    }

    void SwapPipeline::WriteImageDescriptors
    (
        VkDevice device,
        Vk::DescriptorCache& descriptorCache,
        const std::span<Vk::ImageView, Vk::FRAMES_IN_FLIGHT> imageViews
    ) const
    {
        const auto& imageData = GetImageSets(descriptorCache);

        Vk::DescriptorWriter writer = {};

        for (usize i = 0; i < Vk::FRAMES_IN_FLIGHT; ++i)
        {
            writer.WriteImage
            (
                imageData[i].handle,
                0,
                0,
                colorSampler.handle,
                imageViews[i].handle,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
            );
        }

        writer.Update(device);
    }

    const std::array<Vk::DescriptorSet, Vk::FRAMES_IN_FLIGHT>& SwapPipeline::GetImageSets(Vk::DescriptorCache& descriptorCache) const
    {
        return descriptorCache.GetSets(IMAGE_SET_ID);
    }

    Vk::Pipeline SwapPipeline::CreatePipeline(const std::shared_ptr<Vk::Context>& context, VkFormat colorFormat)
    {
        constexpr std::array<VkDynamicState, 2> DYNAMIC_STATES = {VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT, VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT};

        constexpr std::array<VkVertexInputBindingDescription, 1> vertexBindings =
        {
            VkVertexInputBindingDescription
            {
                .binding   = 0,
                .stride    = 2 * sizeof(f32),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
            }
        };

        constexpr std::array<VkVertexInputAttributeDescription, 1> vertexAttribs =
        {
            VkVertexInputAttributeDescription
            {
                .location = 0,
                .binding  = 0,
                .format   = VK_FORMAT_R32G32_SFLOAT,
                .offset   = 0
            }
        };

        std::array<VkFormat, 1> colorFormats = {colorFormat};

        auto colorLayout = context->descriptorCache.AddLayout
        (
            COLOR_LAYOUT_ID,
            context->device,
            Vk::Builders::DescriptorLayoutBuilder()
                .AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
                .Build(context->device)
        );

        auto pipeline = Vk::Builders::PipelineBuilder(context)
            .SetRenderingInfo(colorFormats, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED)
            .AttachShader("Swapchain.vert.spv", VK_SHADER_STAGE_VERTEX_BIT)
            .AttachShader("Swapchain.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
            .SetDynamicStates(DYNAMIC_STATES)
            .SetVertexInputState(vertexBindings, vertexAttribs)
            .SetIAState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, VK_FALSE)
            .SetRasterizerState(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL)
            .SetMSAAState()
            .SetBlendState()
            .AddDescriptorLayout(colorLayout)
            .Build();

        context->descriptorCache.AllocateSets(IMAGE_SET_ID, COLOR_LAYOUT_ID, context->device);

        return pipeline;
    }

    void SwapPipeline::CreatePipelineData(const std::shared_ptr<Vk::Context>& context)
    {
        colorSampler = Vk::Sampler
        (
            context->device,
            {VK_FILTER_NEAREST, VK_FILTER_NEAREST},
            VK_SAMPLER_MIPMAP_MODE_NEAREST,
            {VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE},
            0.0f,
            {VK_FALSE, 0.0f},
            {VK_FALSE, VK_COMPARE_OP_ALWAYS},
            {0.0f, 0.0f},
            VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            VK_FALSE
        );

        constexpr std::array<f32, 12> QUAD_VERTICES =
        {
            -1.0f, -1.0f, // Bottom-left
            -1.0f,  1.0f, // Top-left
             1.0f, -1.0f, // Bottom-right
             1.0f,  1.0f  // Top-right
        };

        screenQuad = Vk::VertexBuffer(context, QUAD_VERTICES);

        m_deletionQueue.PushDeletor([sampler = colorSampler, quad = screenQuad, context] ()
        {
            sampler.Destroy(context->device);
            quad.Destroy(context->allocator);
        });
    }
}