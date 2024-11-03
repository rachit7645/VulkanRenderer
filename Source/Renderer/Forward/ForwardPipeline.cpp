//    Copyright 2023 - 2024 Rachit Khandelwal
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.

#include "ForwardPipeline.h"

#include "Models/Vertex.h"
#include "Vulkan/Builders/PipelineBuilder.h"
#include "Vulkan/DescriptorWriter.h"
#include "Vulkan/Builders/DescriptorLayoutBuilder.h"
#include "ForwardSceneBuffer.h"
#include "Util/Util.h"

namespace Renderer::Forward
{
    // Usings
    using Models::Vertex;

    // Constants
    constexpr auto STATIC_LAYOUT_ID   = "FORWARD_PIPELINE_STATIC_LAYOUT";
    constexpr auto STATIC_SET_ID      = "FORWARD_PIPELINE_STATIC_SETS";

    ForwardPipeline::ForwardPipeline
    (
        Vk::Context& context,
        const Vk::TextureManager& textureManager,
        VkFormat colorFormat,
        VkFormat depthFormat
    )
    {
        CreatePipeline(context, textureManager, colorFormat, depthFormat);
        CreatePipelineData(context);
        WriteStaticDescriptor(context.device, context.descriptorCache);
    }

    Vk::DescriptorSet ForwardPipeline::GetStaticSet(Vk::DescriptorCache& descriptorCache) const
    {
        return descriptorCache.GetSet(STATIC_SET_ID);
    }

    void ForwardPipeline::CreatePipeline
    (
        Vk::Context& context,
        const Vk::TextureManager& textureManager,
        VkFormat colorFormat,
        VkFormat depthFormat
    )
    {
        constexpr std::array DYNAMIC_STATES = {VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT, VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT};

        auto colorFormats = std::span(&colorFormat, 1);

        auto staticLayout = context.descriptorCache.AddLayout
        (
            STATIC_LAYOUT_ID,
            context.device,
            Vk::Builders::DescriptorLayoutBuilder()
                .AddBinding(0, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT)
                .Build(context.device)
        );

        std::tie(handle, layout) = Vk::Builders::PipelineBuilder(context)
            .SetRenderingInfo(colorFormats, depthFormat, VK_FORMAT_UNDEFINED)
            .AttachShader("Forward.vert.spv", VK_SHADER_STAGE_VERTEX_BIT)
            .AttachShader("Forward.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
            .SetDynamicStates(DYNAMIC_STATES)
            .SetVertexInputState(Vertex::GetBindingDescription(), Vertex::GetVertexAttribDescription())
            .SetIAState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE)
            .SetRasterizerState(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL)
            .SetMSAAState()
            .SetDepthStencilState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_GREATER_OR_EQUAL, VK_FALSE, {}, {})
            .SetBlendState()
            .AddPushConstant(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, static_cast<u32>(sizeof(PushConstant)))
            .AddDescriptorLayout(staticLayout)
            .AddDescriptorLayout(textureManager.textureSet.layout)
            .Build();

        context.descriptorCache.AllocateSet(STATIC_SET_ID, STATIC_LAYOUT_ID, context.device);
    }

    void ForwardPipeline::CreatePipelineData(const Vk::Context& context)
    {
        for (auto&& sceneSSBO : sceneSSBOs)
        {
            sceneSSBO = Vk::Buffer
            (
                context.allocator,
                sizeof(SceneBuffer),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                VMA_MEMORY_USAGE_AUTO
            );

            sceneSSBO.GetDeviceAddress(context.device);
        }

        instanceBuffer = Forward::InstanceBuffer(context.device, context.allocator);
        indirectBuffer = Forward::IndirectBuffer(context.allocator);

        auto anisotropy = std::min(4.0f, context.physicalDeviceLimits.maxSamplerAnisotropy);

        textureSampler = Vk::Sampler
        (
            context.device,
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

        m_deletionQueue.PushDeletor([&]()
        {
            instanceBuffer.Destroy(context.allocator);
            indirectBuffer.Destroy(context.allocator);

            for (auto&& buffer : sceneSSBOs)
            {
                buffer.Destroy(context.allocator);
            }

            textureSampler.Destroy(context.device);
        });
    }

    void ForwardPipeline::WriteStaticDescriptor(VkDevice device, Vk::DescriptorCache& cache) const
    {
        auto staticSet = GetStaticSet(cache);

        Vk::DescriptorWriter writer = {};

        writer.WriteImage
        (
            staticSet.handle,
            0,
            0,
            textureSampler.handle,
            VK_NULL_HANDLE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_DESCRIPTOR_TYPE_SAMPLER
        );

        writer.Update(device);
    }
}