/*
 *    Copyright 2023 Rachit Khandelwal
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

#include "PipelineBuilder.h"

#include <utility>
#include <ranges>

#include "ShaderModule.h"
#include "Util/Log.h"

// Usings
using Models::Vertex;

namespace Vk
{
    // TODO: Make everything more customisable

    PipelineBuilder PipelineBuilder::Create(std::shared_ptr<Vk::Context> context, VkRenderPass renderPass)
    {
        // Create
        return PipelineBuilder(std::move(context), renderPass);
    }

    PipelineBuilder::PipelineBuilder(std::shared_ptr<Vk::Context> context, VkRenderPass renderPass)
        : m_context(std::move(context)),
          m_renderPass(renderPass),
          m_vertexInputBindings(Vertex::GetBindingDescription()),
          m_vertexAttribs(Vertex::GetVertexAttribDescription())
    {
    }

    PipelineBuilder::PipelineData PipelineBuilder::Build()
    {
        // Create descriptor set layouts
        auto descriptorLayouts = CreateDescriptorSetLayouts();

        // Pipeline layout create info
        VkPipelineLayoutCreateInfo pipelineLayoutInfo =
        {
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext                  = nullptr,
            .flags                  = 0,
            .setLayoutCount         = static_cast<uint32_t>(descriptorLayouts.size()),
            .pSetLayouts            = descriptorLayouts.data(),
            .pushConstantRangeCount = static_cast<u32>(pushConstantRanges.size()),
            .pPushConstantRanges    = pushConstantRanges.data()
        };

        // Pipeline layout handle
        VkPipelineLayout pipelineLayout = {};

        // Create pipeline layout
        if (vkCreatePipelineLayout(
                m_context->device,
                &pipelineLayoutInfo,
                nullptr,
                &pipelineLayout
            ) != VK_SUCCESS)
        {
            // Log
            Logger::Error("Failed to create pipeline layout! [device={}]\n", reinterpret_cast<void*>(m_context->device));
        }

        // Log
        Logger::Debug("Created pipeline layout! [handle={}]\n", reinterpret_cast<void*>(pipelineLayout));

        // Pipeline creation info
        VkGraphicsPipelineCreateInfo pipelineCreateInfo =
        {
            .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext               = nullptr,
            .flags               = 0,
            .stageCount          = static_cast<u32>(shaderStageCreateInfos.size()),
            .pStages             = shaderStageCreateInfos.data(),
            .pVertexInputState   = &vertexInputInfo                       ,
            .pInputAssemblyState = &inputAssemblyInfo,
            .pTessellationState  = nullptr,
            .pViewportState      = &viewportInfo,
            .pRasterizationState = &rasterizationInfo,
            .pMultisampleState   = &msaaStateInfo,
            .pDepthStencilState  = &depthStencilInfo,
            .pColorBlendState    = &colorBlendInfo,
            .pDynamicState       = &dynamicStateInfo,
            .layout              = pipelineLayout,
            .renderPass          = m_renderPass,
            .subpass             = 0,
            .basePipelineHandle  = VK_NULL_HANDLE,
            .basePipelineIndex   = -1
        };

        // Pipeline object
        VkPipeline pipeline = {};

        // Create pipeline
        if (vkCreateGraphicsPipelines(
                m_context->device,
                nullptr,
                1,
                &pipelineCreateInfo,
                nullptr,
                &pipeline
            ) != VK_SUCCESS)
        {
            // Log
            Logger::Error("Failed to create pipeline! [device={}]\n", reinterpret_cast<void*>(m_context->device));
        }

        // Create descriptor sets
        auto descriptorMap = AllocateDescriptorSets(descriptorLayouts);

        // Log
        Logger::Info("Created pipeline! [handle={}]\n", reinterpret_cast<void*>(pipeline));

        // Return
        return {pipeline, pipelineLayout, descriptorMap};
    }

    PipelineBuilder& PipelineBuilder::AttachShader(const std::string_view path, VkShaderStageFlagBits shaderStage)
    {
        // Load shader module
        auto shaderModule = Vk::CreateShaderModule(m_context->device, path);

        // Pipeline shader stage info
        VkPipelineShaderStageCreateInfo stageCreateInfo =
        {
            .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext               = nullptr,
            .flags               = 0,
            .stage               = shaderStage,
            .module              = shaderModule,
            .pName               = "main",
            .pSpecializationInfo = nullptr
        };

        // Add to vector
        shaderStageCreateInfos.emplace_back(stageCreateInfo);

        // Return
        return *this;
    }

    PipelineBuilder& PipelineBuilder::SetDynamicStates
    (
        const std::span<const VkDynamicState> vkDynamicStates,
        const std::function<void(PipelineBuilder&)>& SetDynStates
    )
    {
        // Set dynamic states
        dynamicStates = std::vector(vkDynamicStates.begin(), vkDynamicStates.end());

        // Set dynamic states creation info
        dynamicStateInfo =
        {
            .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .pNext             = nullptr,
            .flags             = 0,
            .dynamicStateCount = static_cast<u32>(dynamicStates.size()),
            .pDynamicStates    = dynamicStates.data()
        };

        // Set dynamic state data
        SetDynStates(*this);

        // Return
        return *this;
    }

    PipelineBuilder& PipelineBuilder::SetVertexInputState()
    {
        // Set vertex input info
        vertexInputInfo =
        {
            .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext                           = nullptr,
            .flags                           = 0,
            .vertexBindingDescriptionCount   = 1,
            .pVertexBindingDescriptions      = &m_vertexInputBindings,
            .vertexAttributeDescriptionCount = static_cast<u32>(m_vertexAttribs.size()),
            .pVertexAttributeDescriptions    = m_vertexAttribs.data()
        };

        // Return
        return *this;
    }

    PipelineBuilder& PipelineBuilder::SetIAState()
    {
        // IA State
        inputAssemblyInfo =
        {
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext                  = nullptr,
            .flags                  = 0,
            .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE
        };

        // Return
        return *this;
    }

    PipelineBuilder& PipelineBuilder::SetRasterizerState(VkCullModeFlagBits cullMode, VkFrontFace frontFace)
    {
        // Set rasterization info
        rasterizationInfo =
        {
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .depthClampEnable        = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode             = VK_POLYGON_MODE_FILL,
            .cullMode                = cullMode,
            .frontFace               = frontFace,
            .depthBiasEnable         = VK_FALSE,
            .depthBiasConstantFactor = 0.0f,
            .depthBiasClamp          = 0.0f,
            .depthBiasSlopeFactor    = 0.0f,
            .lineWidth               = 1.0f
        };

        // Return
        return *this;
    }

    PipelineBuilder& PipelineBuilder::SetMSAAState()
    {
        // Set MSAA state info
        msaaStateInfo =
        {
            .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable   = VK_FALSE, // Disables MSAA
            .minSampleShading      = 1.0f,
            .pSampleMask           = nullptr,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable      = VK_FALSE
        };

        // Return
        return *this;
    }

    PipelineBuilder& PipelineBuilder::SetDepthStencilState
    (
        VkBool32 depthTestEnable,
        VkBool32 depthWriteEnable,
        VkCompareOp depthCompareOp,
        VkBool32 stencilTestEnable,
        VkStencilOpState front,
        VkStencilOpState back
    )
    {
        // Depth stencil state
        depthStencilInfo =
        {
            .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .depthTestEnable       = depthTestEnable,
            .depthWriteEnable      = depthWriteEnable,
            .depthCompareOp        = depthCompareOp,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable     = stencilTestEnable,
            .front                 = front,
            .back                  = back,
            .minDepthBounds        = 0.0f,
            .maxDepthBounds        = 1.0f
        };

        // Return
        return *this;
    }

    PipelineBuilder& PipelineBuilder::SetBlendState()
    {
        // Create blend attachment state (no alpha)
        VkPipelineColorBlendAttachmentState colorBlendAttachment =
        {
            .blendEnable         = VK_FALSE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
            .colorBlendOp        = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp        = VK_BLEND_OP_ADD,
            .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT |
                                   VK_COLOR_COMPONENT_G_BIT |
                                   VK_COLOR_COMPONENT_B_BIT |
                                   VK_COLOR_COMPONENT_A_BIT
        };

        // Add to states
        colorBlendStates.emplace_back(colorBlendAttachment);

        // Set blend state creation info
        colorBlendInfo =
        {
            .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext           = nullptr,
            .flags           = 0,
            .logicOpEnable   = VK_FALSE,
            .logicOp         = VK_LOGIC_OP_COPY,
            .attachmentCount = static_cast<u32>(colorBlendStates.size()),
            .pAttachments    = colorBlendStates.data(),
            .blendConstants  = {0.0f, 0.0f, 0.0f, 0.0f}
        };

        // Return
        return *this;
    }

    PipelineBuilder& PipelineBuilder::AddPushConstant(VkShaderStageFlags stages, u32 offset, u32 size)
    {
        // Push constant range
        VkPushConstantRange pushConstant =
        {
            .stageFlags = stages,
            .offset     = offset,
            .size       = size
        };

        // Add to vector
        pushConstantRanges.emplace_back(pushConstant);

        // Return
        return *this;
    }

    PipelineBuilder& PipelineBuilder::AddDescriptor(u32 binding, VkDescriptorType type, VkShaderStageFlags stages, usize count)
    {
        // Binding
        VkDescriptorSetLayoutBinding layoutBinding =
        {
            .binding            = binding,
            .descriptorType     = type,
            .descriptorCount    = 1,
            .stageFlags         = stages,
            .pImmutableSamplers = nullptr
        };

        // Add to vector
        descriptorSetBindings.emplace_back(layoutBinding);
        // Add to states
        descriptorStates.emplace_back(type, count);

        // Return
        return *this;
    }

    std::vector<VkDescriptorSetLayout> PipelineBuilder::CreateDescriptorSetLayouts()
    {
        // Descriptor layouts
        std::vector<VkDescriptorSetLayout> descriptorLayouts = {};
        // Reserve
        descriptorLayouts.reserve(descriptorSetBindings.size());

        // Loop
        for (auto& descriptorSetBinding : descriptorSetBindings)
        {
            // Descriptor set layout info
            VkDescriptorSetLayoutCreateInfo layoutInfo =
            {
                .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext        = nullptr,
                .flags        = 0,
                .bindingCount = 1,
                .pBindings    = &descriptorSetBinding
            };

            // Create
            VkDescriptorSetLayout descriptorLayout = {};
            if (vkCreateDescriptorSetLayout(
                    m_context->device,
                    &layoutInfo,
                    nullptr,
                    &descriptorLayout
                ) != VK_SUCCESS)
            {
                // Log
                Logger::Error("Failed to create descriptor layout! [device={}]\n",
                    reinterpret_cast<void*>(m_context->device));
            }

            // Append
            descriptorLayouts.emplace_back(descriptorLayout);
        }

        // NVRO POG
        return descriptorLayouts;
    }

    std::vector<Vk::DescriptorSetData> PipelineBuilder::AllocateDescriptorSets(const std::vector<VkDescriptorSetLayout>& descriptorLayouts)
    {
        // Allocate map
        auto data = std::vector<Vk::DescriptorSetData>();
        data.reserve(descriptorLayouts.size());

        // Loop
        for (usize i = 0; i < descriptorLayouts.size(); ++i)
        {
            // Get state
            auto [type, count] = descriptorStates[i];
            // Duplicate layouts
            auto layouts = std::vector<VkDescriptorSetLayout>(count, descriptorLayouts[i]);
            // Allocate sets
            auto sets = m_context->AllocateDescriptorSets(layouts);
            // Insert into map
            data.emplace_back(descriptorSetBindings[i].binding, type, descriptorLayouts[i], sets);
        }

        // Return (NVRO please work)
        return data;
    }

    PipelineBuilder::~PipelineBuilder()
    {
        // Loop over all shader stages
        for (auto&& stage : shaderStageCreateInfos)
        {
            // Log
            Logger::Debug("Destroying shader module [handle={}]\n", reinterpret_cast<void*>(stage.module));
            // Destroy
            vkDestroyShaderModule(m_context->device, stage.module, nullptr);
        }
    }
}