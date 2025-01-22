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

#include "SwapchainPipeline.h"

#include "Vulkan/Builders/PipelineBuilder.h"
#include "Util/Log.h"
#include "Vulkan/DescriptorWriter.h"
#include "Vulkan/Builders/DescriptorLayoutBuilder.h"

namespace Renderer::Swapchain
{
    SwapchainPipeline::SwapchainPipeline
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

    void SwapchainPipeline::WriteColorAttachmentDescriptor
    (
        VkDevice device,
        Vk::MegaSet& megaSet,
        const Vk::ImageView& imageView
    )
    {
        colorAttachmentIndex = megaSet.WriteImage(imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        megaSet.Update(device);
    }

    void SwapchainPipeline::CreatePipeline(const Vk::Context& context, Vk::MegaSet& megaSet, VkFormat colorFormat)
    {
        constexpr std::array DYNAMIC_STATES = {VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT, VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT};

        std::array colorFormats = {colorFormat};

        std::tie(handle, layout) = Vk::Builders::PipelineBuilder(context)
            .SetRenderingInfo(colorFormats, VK_FORMAT_UNDEFINED, VK_FORMAT_UNDEFINED)
            .AttachShader("Swapchain.vert.spv", VK_SHADER_STAGE_VERTEX_BIT)
            .AttachShader("Swapchain.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
            .SetDynamicStates(DYNAMIC_STATES)
            .SetVertexInputState({}, {})
            .SetIAState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE)
            .SetRasterizerState(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL)
            .SetMSAAState()
            .SetBlendState()
            .AddPushConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0, static_cast<u32>(sizeof(Swapchain::PushConstant)))
            .AddDescriptorLayout(megaSet.descriptorSet.layout)
            .Build();

        #ifdef ENGINE_DEBUG
        VkDebugUtilsObjectNameInfoEXT nameInfo =
        {
            .sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .pNext        = nullptr,
            .objectType   = VK_OBJECT_TYPE_UNKNOWN,
            .objectHandle = 0,
            .pObjectName  = nullptr
        };

        nameInfo.objectType   = VK_OBJECT_TYPE_PIPELINE;
        nameInfo.objectHandle = std::bit_cast<u64>(handle);
        nameInfo.pObjectName  = "SwapchainPipeline";
        vkSetDebugUtilsObjectNameEXT(context.device, &nameInfo);

        nameInfo.objectType   = VK_OBJECT_TYPE_PIPELINE_LAYOUT;
        nameInfo.objectHandle = std::bit_cast<u64>(layout);
        nameInfo.pObjectName  = "SwapchainPipelineLayout";
        vkSetDebugUtilsObjectNameEXT(context.device, &nameInfo);
        #endif
    }

    void SwapchainPipeline::CreatePipelineData(VkDevice device, Vk::MegaSet& megaSet, Vk::TextureManager& textureManager)
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

        #ifdef ENGINE_DEBUG
        VkDebugUtilsObjectNameInfoEXT nameInfo =
        {
            .sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .pNext        = nullptr,
            .objectType   = VK_OBJECT_TYPE_SAMPLER,
            .objectHandle = std::bit_cast<u64>(textureManager.GetSampler(samplerIndex).handle),
            .pObjectName  = "SwapchainPipeline/Sampler"
        };

        vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
        #endif

        megaSet.Update(device);
    }
}