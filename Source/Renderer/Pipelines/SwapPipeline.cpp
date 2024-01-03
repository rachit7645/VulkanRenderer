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

namespace Renderer::Pipelines
{
    SwapPipeline::SwapPipeline
    (
        const std::shared_ptr<Vk::Context>& context,
        VkFormat colorFormat,
        VkExtent2D extent
    )
        : Vk::Pipeline(CreatePipeline(context, colorFormat, extent))
    {
        CreatePipelineData(context);
    }

    void SwapPipeline::WriteImageDescriptors(VkDevice device, const std::span<Vk::ImageView, Vk::FRAMES_IN_FLIGHT> imageViews)
    {
        const auto& imageData = GetImageData();

        std::array<VkDescriptorImageInfo, Vk::FRAMES_IN_FLIGHT> imageInfos  = {};
        std::array<VkWriteDescriptorSet,  Vk::FRAMES_IN_FLIGHT> imageWrites = {};

        for (usize i = 0; i < Vk::FRAMES_IN_FLIGHT; ++i)
        {
            imageInfos[i] =
            {
                .sampler     = textureSampler.handle,
                .imageView   = imageViews[i].handle,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };

            imageWrites[i] =
            {
                .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext            = nullptr,
                .dstSet           = imageData.setMap[i][0],
                .dstBinding       = imageData.binding,
                .dstArrayElement  = 0,
                .descriptorCount  = 1,
                .descriptorType   = imageData.type,
                .pImageInfo       = &imageInfos[i],
                .pBufferInfo      = nullptr,
                .pTexelBufferView = nullptr
            };
        }

        vkUpdateDescriptorSets
        (
            device,
            static_cast<u32>(imageWrites.size()),
            imageWrites.data(),
            0,
            nullptr
        );
    }

    const Vk::DescriptorSetData& SwapPipeline::GetImageData() const
    {
        return descriptorSetData[0];
    }

    Vk::Pipeline SwapPipeline::CreatePipeline
    (
        const std::shared_ptr<Vk::Context>& context,
        VkFormat colorFormat,
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

        constexpr std::array<VkDynamicState, 2> DYN_STATES = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

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

        return Vk::Builders::PipelineBuilder(context)
              .SetRenderingInfo(colorFormats, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED)
              .AttachShader("Swapchain.vert.spv", VK_SHADER_STAGE_VERTEX_BIT)
              .AttachShader("Swapchain.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
              .SetDynamicStates(DYN_STATES, SetDynamicStates)
              .SetVertexInputState(vertexBindings, vertexAttribs)
              .SetIAState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, VK_FALSE)
              .SetRasterizerState(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL)
              .SetMSAAState()
              .SetBlendState()
              .AddDescriptor(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1, 1)
              .Build();
    }

    void SwapPipeline::CreatePipelineData(const std::shared_ptr<Vk::Context>& context)
    {
        textureSampler = Vk::Sampler
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
    }

    void SwapPipeline::DestroyPipelineData(VkDevice device, VmaAllocator allocator)
    {
        textureSampler.Destroy(device);
        screenQuad.Destroy(allocator);
    }
}