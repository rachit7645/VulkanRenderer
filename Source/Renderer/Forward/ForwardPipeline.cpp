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

#include "ForwardPipeline.h"

#include "Models/Vertex.h"
#include "Vulkan/Builders/PipelineBuilder.h"
#include "ForwardSceneBuffer.h"
#include "Util/Util.h"

namespace Renderer::Forward
{
    // Usings
    using Models::Vertex;

    ForwardPipeline::ForwardPipeline
    (
        const Vk::Context& context,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager,
        VkFormat colorFormat,
        VkFormat depthFormat
    )
    {
        CreatePipeline(context, megaSet, colorFormat, depthFormat);
        CreatePipelineData(context, megaSet, textureManager);
    }

    void ForwardPipeline::CreatePipeline
    (
        const Vk::Context& context,
        const Vk::MegaSet& megaSet,
        VkFormat colorFormat,
        VkFormat depthFormat
    )
    {
        constexpr std::array DYNAMIC_STATES = {VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT, VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT};

        auto colorFormats = std::span(&colorFormat, 1);

        std::tie(handle, layout) = Vk::Builders::PipelineBuilder(context)
            .SetRenderingInfo(colorFormats, depthFormat, VK_FORMAT_UNDEFINED)
            .AttachShader("Forward.vert.spv", VK_SHADER_STAGE_VERTEX_BIT)
            .AttachShader("Forward.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
            .SetDynamicStates(DYNAMIC_STATES)
            .SetVertexInputState({}, {})
            .SetIAState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE)
            .SetRasterizerState(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL)
            .SetMSAAState()
            .SetDepthStencilState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_FALSE, {}, {})
            .SetBlendState()
            .AddPushConstant(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, static_cast<u32>(sizeof(PushConstant)))
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
        nameInfo.pObjectName  = "ForwardPipeline";
        vkSetDebugUtilsObjectNameEXT(context.device, &nameInfo);

        nameInfo.objectType   = VK_OBJECT_TYPE_PIPELINE_LAYOUT;
        nameInfo.objectHandle = std::bit_cast<u64>(layout);
        nameInfo.pObjectName  = "ForwardPipelineLayout";
        vkSetDebugUtilsObjectNameEXT(context.device, &nameInfo);
        #endif
    }

    void ForwardPipeline::CreatePipelineData
    (
        const Vk::Context& context,
        Vk::MegaSet& megaSet,
        Vk::TextureManager& textureManager
    )
    {
        for (usize i = 0; i < sceneBuffers.size(); ++i)
        {
            sceneBuffers[i] = Vk::Buffer
            (
                context.allocator,
                sizeof(SceneBuffer),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            sceneBuffers[i].GetDeviceAddress(context.device);

            #ifdef ENGINE_DEBUG
            auto name = fmt::format("SceneBuffer/{}", i);

            VkDebugUtilsObjectNameInfoEXT nameInfo =
            {
                .sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .pNext        = nullptr,
                .objectType   = VK_OBJECT_TYPE_BUFFER,
                .objectHandle = std::bit_cast<u64>(sceneBuffers[i].handle),
                .pObjectName  = name.c_str()
            };

            vkSetDebugUtilsObjectNameEXT(context.device, &nameInfo);
            #endif
        }

        meshBuffer     = Forward::MeshBuffer(context.device, context.allocator);
        indirectBuffer = Forward::IndirectBuffer(context.device, context.allocator);

        auto anisotropy = std::min(16.0f, context.physicalDeviceLimits.maxSamplerAnisotropy);

        samplerIndex = textureManager.AddSampler
        (
            megaSet,
            context.device,
            {
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
                .borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
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
            .pObjectName  = "ForwardPipeline/Sampler"
        };

        vkSetDebugUtilsObjectNameEXT(context.device, &nameInfo);
        #endif

        megaSet.Update(context.device);

        m_deletionQueue.PushDeletor([&]()
        {
            meshBuffer.Destroy(context.allocator);
            indirectBuffer.Destroy(context.allocator);

            for (auto&& buffer : sceneBuffers)
            {
                buffer.Destroy(context.allocator);
            }
        });
    }
}