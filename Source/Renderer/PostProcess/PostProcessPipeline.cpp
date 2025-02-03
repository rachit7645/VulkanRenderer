/*
 * Copyright (c) 2023 - 2025 Rachit
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "PostProcessPipeline.h"

#include "Vulkan/Builders/PipelineBuilder.h"
#include "Util/Log.h"
#include "Vulkan/DebugUtils.h"

namespace Renderer::PostProcess
{
    PostProcessPipeline::PostProcessPipeline
    (
        const Vk::Context& context,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager,
        VkFormat colorFormat
    )
    {
        CreatePipeline(context, megaSet, colorFormat);
        CreatePipelineData(context.device, megaSet, textureManager);
    }

    void PostProcessPipeline::CreatePipeline(const Vk::Context& context, const Vk::MegaSet& megaSet, VkFormat colorFormat)
    {
        constexpr std::array DYNAMIC_STATES = {VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT, VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT};

        std::array colorFormats = {colorFormat};

        std::tie(handle, layout, bindPoint) = Vk::Builders::PipelineBuilder(context)
            .SetPipelineType(VK_PIPELINE_BIND_POINT_GRAPHICS)
            .SetRenderingInfo(colorFormats, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED)
            .AttachShader("PostProcess.vert.spv", VK_SHADER_STAGE_VERTEX_BIT)
            .AttachShader("PostProcess.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
            .SetDynamicStates(DYNAMIC_STATES)
            .SetIAState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE)
            .SetRasterizerState(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL)
            .SetMSAAState()
            .AddBlendAttachment(
                VK_FALSE,
                VK_BLEND_FACTOR_ONE,
                VK_BLEND_FACTOR_ZERO,
                VK_BLEND_OP_ADD,
                VK_BLEND_FACTOR_ONE,
                VK_BLEND_FACTOR_ZERO,
                VK_BLEND_OP_ADD,
                VK_COLOR_COMPONENT_R_BIT |
                VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT |
                VK_COLOR_COMPONENT_A_BIT
            )
            .SetBlendState()
            .AddPushConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PostProcess::PushConstant))
            .AddDescriptorLayout(megaSet.descriptorSet.layout)
            .Build();

        Vk::SetDebugName(context.device, handle, "SwapchainPipeline");
        Vk::SetDebugName(context.device, layout, "SwapchainPipelineLayout");
    }

    void PostProcessPipeline::CreatePipelineData(VkDevice device, Vk::MegaSet& megaSet, Vk::TextureManager& textureManager)
    {
        samplerIndex = textureManager.AddSampler
        (
            megaSet,
            device,
            {
                .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .pNext                   = nullptr,
                .flags                   = 0,
                .magFilter               = VK_FILTER_NEAREST,
                .minFilter               = VK_FILTER_NEAREST,
                .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                .addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .mipLodBias              = 0.0f,
                .anisotropyEnable        = VK_FALSE,
                .maxAnisotropy           = 0.0f,
                .compareEnable           = VK_FALSE,
                .compareOp               = VK_COMPARE_OP_ALWAYS,
                .minLod                  = 0.0f,
                .maxLod                  = 0.0f,
                .borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
                .unnormalizedCoordinates = VK_FALSE
            }
        );

        Vk::SetDebugName(device, textureManager.GetSampler(samplerIndex).handle, "SwapchainPipeline/Sampler");

        megaSet.Update(device);
    }
}