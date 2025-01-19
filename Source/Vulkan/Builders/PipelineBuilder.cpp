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

#include "PipelineBuilder.h"

#include "Util/Log.h"
#include "Vulkan/Util.h"

namespace Vk::Builders
{
    PipelineBuilder::Products PipelineBuilder::Build()
    {
        const VkPipelineLayoutCreateInfo pipelineLayoutInfo =
        {
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext                  = nullptr,
            .flags                  = 0,
            .setLayoutCount         = static_cast<u32>(descriptorLayouts.size()),
            .pSetLayouts            = descriptorLayouts.data(),
            .pushConstantRangeCount = static_cast<u32>(pushConstantRanges.size()),
            .pPushConstantRanges    = pushConstantRanges.data()
        };

        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        Vk::CheckResult(vkCreatePipelineLayout(
            m_context->device,
            &pipelineLayoutInfo,
            nullptr,
            &pipelineLayout),
            "Failed to create pipeline layout!"
        );

        const VkGraphicsPipelineCreateInfo pipelineCreateInfo =
        {
            .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext               = &renderingCreateInfo,
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
            .renderPass          = VK_NULL_HANDLE,
            .subpass             = 0,
            .basePipelineHandle  = VK_NULL_HANDLE,
            .basePipelineIndex   = -1
        };

        VkPipeline pipeline = VK_NULL_HANDLE;
        Vk::CheckResult(vkCreateGraphicsPipelines(
            m_context->device,
            nullptr,
            1,
            &pipelineCreateInfo,
            nullptr,
            &pipeline),
            "Failed to create pipeline!"
        );

        Logger::Debug("Created pipeline! [handle={}]\n", std::bit_cast<void*>(pipeline));

        return {pipeline, pipelineLayout};
    }

    PipelineBuilder::PipelineBuilder(const Vk::Context& context)
        : m_context(&context)
    {
    }

    PipelineBuilder& PipelineBuilder::SetRenderingInfo
    (
        const std::span<const VkFormat> colorFormats,
        VkFormat depthFormat,
        VkFormat stencilFormat
    )
    {
        renderingColorFormats = std::vector(colorFormats.begin(), colorFormats.end());

        renderingCreateInfo =
        {
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .pNext                   = nullptr,
            .viewMask                = 0,
            .colorAttachmentCount    = static_cast<u32>(renderingColorFormats.size()),
            .pColorAttachmentFormats = renderingColorFormats.data(),
            .depthAttachmentFormat   = depthFormat,
            .stencilAttachmentFormat = stencilFormat
        };

        return *this;
    }

    PipelineBuilder& PipelineBuilder::AttachShader(const std::string_view path, VkShaderStageFlagBits shaderStage)
    {
        shaderModules.emplace_back(m_context->device, path);

        const VkPipelineShaderStageCreateInfo stageCreateInfo =
        {
            .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext               = nullptr,
            .flags               = 0,
            .stage               = shaderStage,
            .module              = shaderModules.back().handle,
            .pName               = "main",
            .pSpecializationInfo = nullptr
        };

        shaderStageCreateInfos.emplace_back(stageCreateInfo);

        return *this;
    }

    PipelineBuilder& PipelineBuilder::SetDynamicStates(const std::span<const VkDynamicState> vkDynamicStates)
    {
        dynamicStates = std::vector(vkDynamicStates.begin(), vkDynamicStates.end());

        dynamicStateInfo =
        {
            .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .pNext             = nullptr,
            .flags             = 0,
            .dynamicStateCount = static_cast<u32>(dynamicStates.size()),
            .pDynamicStates    = dynamicStates.data()
        };

        viewportInfo =
        {
            .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext         = nullptr,
            .flags         = 0,
            .viewportCount = 0,
            .pViewports    = nullptr,
            .scissorCount  = 0,
            .pScissors     = nullptr
        };

        return *this;
    }

    PipelineBuilder& PipelineBuilder::SetVertexInputState
    (
        const std::span<const VkVertexInputBindingDescription> vertexBindings,
        const std::span<const VkVertexInputAttributeDescription> vertexAttribs
    )
    {
        vertexInputBindings      = std::vector(vertexBindings.begin(), vertexBindings.end());
        vertexAttribDescriptions = std::vector(vertexAttribs.begin(), vertexAttribs.end());

        vertexInputInfo =
        {
            .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext                           = nullptr,
            .flags                           = 0,
            .vertexBindingDescriptionCount   = static_cast<u32>(vertexInputBindings.size()),
            .pVertexBindingDescriptions      = vertexInputBindings.data(),
            .vertexAttributeDescriptionCount = static_cast<u32>(vertexAttribDescriptions.size()),
            .pVertexAttributeDescriptions    = vertexAttribDescriptions.data()
        };

        return *this;
    }

    PipelineBuilder& PipelineBuilder::SetRasterizerState(VkCullModeFlags cullMode, VkFrontFace frontFace, VkPolygonMode polygonMode)
    {
        rasterizationInfo =
        {
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .depthClampEnable        = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode             = polygonMode,
            .cullMode                = cullMode,
            .frontFace               = frontFace,
            .depthBiasEnable         = VK_FALSE,
            .depthBiasConstantFactor = 0.0f,
            .depthBiasClamp          = 0.0f,
            .depthBiasSlopeFactor    = 0.0f,
            .lineWidth               = 1.0f
        };

        return *this;
    }

    PipelineBuilder& PipelineBuilder::SetIAState(VkPrimitiveTopology topology, VkBool32 enablePrimitiveRestart)
    {
        inputAssemblyInfo =
        {
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext                  = nullptr,
            .flags                  = 0,
            .topology               = topology,
            .primitiveRestartEnable = enablePrimitiveRestart
        };

        return *this;
    }

    // TODO: Add customisable MSAA
    PipelineBuilder& PipelineBuilder::SetMSAAState()
    {
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

        return *this;
    }

    PipelineBuilder& PipelineBuilder::SetDepthStencilState
    (
        VkBool32 depthTestEnable,
        VkBool32 depthWriteEnable,
        VkCompareOp depthCompareOp,
        VkBool32 stencilTestEnable,
        const VkStencilOpState& front,
        const VkStencilOpState& back
    )
    {
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

        return *this;
    }

    // TODO: Add customisable blending
    PipelineBuilder& PipelineBuilder::SetBlendState()
    {
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

        colorBlendStates.emplace_back(colorBlendAttachment);

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

        return *this;
    }

    PipelineBuilder& PipelineBuilder::AddPushConstant(VkShaderStageFlags stages, u32 offset, u32 size)
    {
        const VkPushConstantRange pushConstant =
        {
            .stageFlags = stages,
            .offset     = offset,
            .size       = size
        };

        pushConstantRanges.emplace_back(pushConstant);

        return *this;
    }

    PipelineBuilder& PipelineBuilder::AddDescriptorLayout(VkDescriptorSetLayout layout)
    {
        descriptorLayouts.emplace_back(layout);

        return *this;
    }

    PipelineBuilder::~PipelineBuilder()
    {
        for (const auto& shaderModule : shaderModules)
        {
            shaderModule.Destroy(m_context->device);
        }
    }
}