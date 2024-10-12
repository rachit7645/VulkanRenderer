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

#include "SwapchainPipeline.h"

#include "Vulkan/Builders/PipelineBuilder.h"
#include "Util/Log.h"
#include "Vulkan/DescriptorWriter.h"
#include "Vulkan/Builders/DescriptorLayoutBuilder.h"

namespace Renderer::Pipelines
{
    constexpr auto COLOR_LAYOUT_ID = "SWAPCHAIN_PIPELINE_COLOR_LAYOUT";
    constexpr auto IMAGE_SET_ID    = "SWAPCHAIN_PIPELINE_IMAGE_SETS";

    SwapchainPipeline::SwapchainPipeline(Vk::Context& context, VkFormat colorFormat)
    {
        CreatePipeline(context, colorFormat);
        CreatePipelineData(context);
    }

    void SwapchainPipeline::WriteImageDescriptors
    (
        VkDevice device,
        Vk::DescriptorCache& descriptorCache,
        const Vk::ImageView& imageView
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
                imageView.handle,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
            );
        }

        writer.Update(device);
    }

    const std::array<Vk::DescriptorSet, Vk::FRAMES_IN_FLIGHT>& SwapchainPipeline::GetImageSets(Vk::DescriptorCache& descriptorCache) const
    {
        return descriptorCache.GetSets(IMAGE_SET_ID);
    }

    void SwapchainPipeline::CreatePipeline(Vk::Context& context, VkFormat colorFormat)
    {
        constexpr std::array<VkDynamicState, 2> DYNAMIC_STATES = {VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT, VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT};

        std::array<VkFormat, 1> colorFormats = {colorFormat};

        auto colorLayout = context.descriptorCache.AddLayout
        (
            COLOR_LAYOUT_ID,
            context.device,
            Vk::Builders::DescriptorLayoutBuilder()
                .AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
                .Build(context.device)
        );

        std::tie(handle, layout) = Vk::Builders::PipelineBuilder(context)
            .SetRenderingInfo(colorFormats, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED)
            .AttachShader("Swapchain.vert.spv", VK_SHADER_STAGE_VERTEX_BIT)
            .AttachShader("Swapchain.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
            .SetDynamicStates(DYNAMIC_STATES)
            .SetVertexInputState({}, {})
            .SetIAState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE)
            .SetRasterizerState(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL)
            .SetMSAAState()
            .SetBlendState()
            .AddDescriptorLayout(colorLayout)
            .Build();

        context.descriptorCache.AllocateSets(IMAGE_SET_ID, COLOR_LAYOUT_ID, context.device);
    }

    void SwapchainPipeline::CreatePipelineData(const Vk::Context& context)
    {
        colorSampler = Vk::Sampler
        (
            context.device,
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

        m_deletionQueue.PushDeletor([sampler = colorSampler, &context] ()
        {
            sampler.Destroy(context.device);
        });
    }
}