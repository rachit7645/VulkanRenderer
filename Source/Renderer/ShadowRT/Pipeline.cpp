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
#include "Vulkan/PipelineBuilder.h"
#include "Vulkan/DebugUtils.h"
#include "Shadows/ShadowRT.h"

namespace Renderer::ShadowRT
{
    Pipeline::Pipeline
    (
        const Vk::Context& context,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager
    )
    {
        std::tie(handle, layout, bindPoint) = Vk::PipelineBuilder(context)
            .SetPipelineType(VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR)
            .AttachShader("Shadows/RT/Shadow.rgen",  VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .AttachShader("Shadows/RT/Shadow.rmiss", VK_SHADER_STAGE_MISS_BIT_KHR)
            .AttachShader("Shadows/RT/Shadow.rahit", VK_SHADER_STAGE_ANY_HIT_BIT_KHR)
            .AttachShaderGroup(VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,             0,                    VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR)
            .AttachShaderGroup(VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,             1,                    VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR)
            .AttachShaderGroup(VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, 2                   )
            .SetMaxRayRecursionDepth(1)
            .AddPushConstant(VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, 0, sizeof(ShadowRT::Constants))
            .AddDescriptorLayout(megaSet.descriptorLayout)
            .Build();

        gBufferSamplerIndex = textureManager.AddSampler
        (
            megaSet,
            context.device,
            VkSamplerCreateInfo{
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
                .borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
                .unnormalizedCoordinates = VK_FALSE
            }
        );

        const auto anisotropy = std::min(16.0f, context.physicalDeviceLimits.maxSamplerAnisotropy);

        textureSamplerIndex = textureManager.AddSampler
        (
            megaSet,
            context.device,
            VkSamplerCreateInfo{
                .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .pNext                   = nullptr,
                .flags                   = 0,
                .magFilter               = VK_FILTER_LINEAR,
                .minFilter               = VK_FILTER_LINEAR,
                .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                .addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                .addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                .addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                .mipLodBias              = 0.0f,
                .anisotropyEnable        = VK_TRUE,
                .maxAnisotropy           = anisotropy,
                .compareEnable           = VK_FALSE,
                .compareOp               = VK_COMPARE_OP_ALWAYS,
                .minLod                  = 0.0f,
                .maxLod                  = VK_LOD_CLAMP_NONE,
                .borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
                .unnormalizedCoordinates = VK_FALSE
            }
        );

        Vk::SetDebugName(context.device, handle,                                                "ShadowRT/Pipeline");
        Vk::SetDebugName(context.device, layout,                                                "ShadowRT/Pipeline/Layout");
        Vk::SetDebugName(context.device, textureManager.GetSampler(gBufferSamplerIndex).handle, "ShadowRT/Pipeline/GBufferSampler");
        Vk::SetDebugName(context.device, textureManager.GetSampler(textureSamplerIndex).handle, "ShadowRT/Pipeline/TextureSampler");
    }
}