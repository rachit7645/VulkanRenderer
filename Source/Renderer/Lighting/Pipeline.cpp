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

#include "Pipeline.h"

#include "Models/Vertex.h"
#include "Vulkan/PipelineBuilder.h"
#include "Vulkan/DebugUtils.h"
#include "Util/Util.h"

namespace Renderer::Lighting
{
    Pipeline::Pipeline
    (
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager
    )
    {
        CreatePipeline(context, formatHelper, megaSet);
        CreatePipelineData(context.device, megaSet, textureManager);
    }

    void Pipeline::CreatePipeline
    (
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        const Vk::MegaSet& megaSet
    )
    {
        constexpr std::array DYNAMIC_STATES = {VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT, VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT};

        const std::array colorFormats = {formatHelper.colorAttachmentFormatHDR};

        std::tie(handle, layout, bindPoint) = Vk::PipelineBuilder(context)
            .SetPipelineType(VK_PIPELINE_BIND_POINT_GRAPHICS)
            .SetRenderingInfo(0, colorFormats, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED)
            .AttachShader("Misc/Trongle.vert",      VK_SHADER_STAGE_VERTEX_BIT)
            .AttachShader("Deferred/Lighting.frag", VK_SHADER_STAGE_FRAGMENT_BIT)
            .SetDynamicStates(DYNAMIC_STATES)
            .SetIAState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE)
            .SetRasterizerState(VK_FALSE, VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL)
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
            .AddPushConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0, static_cast<u32>(sizeof(PushConstant)))
            .AddDescriptorLayout(megaSet.descriptorLayout)
            .Build();

        Vk::SetDebugName(context.device, handle, "LightingPipeline");
        Vk::SetDebugName(context.device, layout, "LightingPipelineLayout");
    }

    void Pipeline::CreatePipelineData
    (
        VkDevice device,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager
    )
    {
        gBufferSamplerIndex = textureManager.AddSampler
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
                .maxLod                  = VK_LOD_CLAMP_NONE,
                .borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
                .unnormalizedCoordinates = VK_FALSE
            }
        );

        iblSamplerIndex = textureManager.AddSampler
        (
            megaSet,
            device,
            {
                .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .pNext                   = nullptr,
                .flags                   = 0,
                .magFilter               = VK_FILTER_LINEAR,
                .minFilter               = VK_FILTER_LINEAR,
                .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                .addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .mipLodBias              = 0.0f,
                .anisotropyEnable        = VK_FALSE,
                .maxAnisotropy           = 1.0f,
                .compareEnable           = VK_FALSE,
                .compareOp               = VK_COMPARE_OP_ALWAYS,
                .minLod                  = 0.0f,
                .maxLod                  = 5.0f,
                .borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                .unnormalizedCoordinates = VK_FALSE
            }
        );

        shadowSamplerIndex = textureManager.AddSampler
        (
            megaSet,
            device,
            {
                .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .pNext                   = nullptr,
                .flags                   = 0,
                .magFilter               = VK_FILTER_LINEAR,
                .minFilter               = VK_FILTER_LINEAR,
                .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                .addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                .addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                .addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                .mipLodBias              = 0.0f,
                .anisotropyEnable        = VK_FALSE,
                .maxAnisotropy           = 1.0f,
                .compareEnable           = VK_TRUE,
                .compareOp               = VK_COMPARE_OP_LESS,
                .minLod                  = 0.0f,
                .maxLod                  = VK_LOD_CLAMP_NONE,
                .borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
                .unnormalizedCoordinates = VK_FALSE
            }
        );

        Vk::SetDebugName(device, textureManager.GetSampler(gBufferSamplerIndex).handle, "LightingPipeline/GBufferSampler");
        Vk::SetDebugName(device, textureManager.GetSampler(iblSamplerIndex).handle,     "LightingPipeline/IBLSampler");
        Vk::SetDebugName(device, textureManager.GetSampler(shadowSamplerIndex).handle,  "LightingPipeline/ShadowSampler");

        megaSet.Update(device);
    }
}