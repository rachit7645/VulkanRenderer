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
            .setLayoutCount         = static_cast<u32>(m_descriptorLayouts.size()),
            .pSetLayouts            = m_descriptorLayouts.data(),
            .pushConstantRangeCount = static_cast<u32>(m_pushConstantRanges.size()),
            .pPushConstantRanges    = m_pushConstantRanges.data()
        };

        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        Vk::CheckResult(vkCreatePipelineLayout(
            m_context->device,
            &pipelineLayoutInfo,
            nullptr,
            &pipelineLayout),
            "Failed to create pipeline layout!"
        );

        VkPipeline pipeline = VK_NULL_HANDLE;

        switch (m_pipelineType)
        {
        case VK_PIPELINE_BIND_POINT_GRAPHICS:
        {
            const VkGraphicsPipelineCreateInfo pipelineCreateInfo =
            {
                .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .pNext               = &m_renderingCreateInfo,
                .flags               = 0,
                .stageCount          = static_cast<u32>(m_shaderStageCreateInfos.size()),
                .pStages             = m_shaderStageCreateInfos.data(),
                .pVertexInputState   = &m_vertexInputInfo                       ,
                .pInputAssemblyState = &m_inputAssemblyInfo,
                .pTessellationState  = nullptr,
                .pViewportState      = &m_viewportInfo,
                .pRasterizationState = &m_rasterizationInfo,
                .pMultisampleState   = &m_msaaStateInfo,
                .pDepthStencilState  = &m_depthStencilInfo,
                .pColorBlendState    = &m_colorBlendInfo,
                .pDynamicState       = &m_dynamicStateInfo,
                .layout              = pipelineLayout,
                .renderPass          = VK_NULL_HANDLE,
                .subpass             = 0,
                .basePipelineHandle  = VK_NULL_HANDLE,
                .basePipelineIndex   = -1
            };

            Vk::CheckResult(vkCreateGraphicsPipelines(
                m_context->device,
                nullptr,
                1,
                &pipelineCreateInfo,
                nullptr,
                &pipeline),
                "Failed to create graphics pipeline!"
            );

            Logger::Debug("Created graphics pipeline! [handle={}]\n", std::bit_cast<void*>(pipeline));
        }
        break;

        case VK_PIPELINE_BIND_POINT_COMPUTE:
        {
            const VkComputePipelineCreateInfo pipelineCreateInfo =
            {
                .sType              = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                .pNext              = nullptr,
                .flags              = 0,
                .stage              = m_shaderStageCreateInfos[0],
                .layout             = pipelineLayout,
                .basePipelineHandle = VK_NULL_HANDLE,
                .basePipelineIndex  = -1
            };

            Vk::CheckResult(vkCreateComputePipelines(
                m_context->device,
                nullptr,
                1,
                &pipelineCreateInfo,
                nullptr,
                &pipeline),
                "Failed to create compute pipeline!"
            );

            Logger::Debug("Created compute pipeline! [handle={}]\n", std::bit_cast<void*>(pipeline));
        }
        break;

        default:
            Logger::Error("{}\n", "Invalid pipeline type!");
        }

        return {pipeline, pipelineLayout, m_pipelineType};
    }

    PipelineBuilder& PipelineBuilder::SetPipelineType(VkPipelineBindPoint bindPoint)
    {
        m_pipelineType = bindPoint;

        return *this;
    }

    PipelineBuilder::PipelineBuilder(const Vk::Context& context)
        : m_context(&context)
    {
        // Disable warnings
        #if defined(__GNUC__) && !defined(__clang__)
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
        #elif defined(__clang__)
        #pragma clang diagnostic push
        #pragma clang diagnostic ignored "-Wmissing-field-initializers"
        #endif

        m_renderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO           };
        m_dynamicStateInfo    = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO       };
        m_viewportInfo        = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO      };
        m_vertexInputInfo     = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO  };
        m_inputAssemblyInfo   = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        m_rasterizationInfo   = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        m_msaaStateInfo       = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO   };
        m_depthStencilInfo    = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
        m_colorBlendInfo      = {.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO   };

        // Reset warning stack
        #if defined(__GNUC__) && !defined(__clang__)
        #pragma GCC diagnostic pop
        #elif defined(__clang__)
        #pragma clang diagnostic pop
        #endif
    }

    PipelineBuilder& PipelineBuilder::SetRenderingInfo
    (
        const std::span<const VkFormat> colorFormats,
        VkFormat depthFormat,
        VkFormat stencilFormat
    )
    {
        m_renderingColorFormats = std::vector(colorFormats.begin(), colorFormats.end());

        m_renderingCreateInfo =
        {
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .pNext                   = nullptr,
            .viewMask                = 0,
            .colorAttachmentCount    = static_cast<u32>(m_renderingColorFormats.size()),
            .pColorAttachmentFormats = m_renderingColorFormats.data(),
            .depthAttachmentFormat   = depthFormat,
            .stencilAttachmentFormat = stencilFormat
        };

        return *this;
    }

    PipelineBuilder& PipelineBuilder::AttachShader(const std::string_view path, VkShaderStageFlagBits shaderStage)
    {
        m_shaderModules.emplace_back(m_context->device, path);

        const VkPipelineShaderStageCreateInfo stageCreateInfo =
        {
            .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext               = nullptr,
            .flags               = 0,
            .stage               = shaderStage,
            .module              = m_shaderModules.back().handle,
            .pName               = "main",
            .pSpecializationInfo = nullptr
        };

        m_shaderStageCreateInfos.emplace_back(stageCreateInfo);

        return *this;
    }

    PipelineBuilder& PipelineBuilder::SetDynamicStates(const std::span<const VkDynamicState> vkDynamicStates)
    {
        m_dynamicStates = std::vector(vkDynamicStates.begin(), vkDynamicStates.end());

        m_dynamicStateInfo =
        {
            .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .pNext             = nullptr,
            .flags             = 0,
            .dynamicStateCount = static_cast<u32>(m_dynamicStates.size()),
            .pDynamicStates    = m_dynamicStates.data()
        };

        return *this;
    }

    PipelineBuilder& PipelineBuilder::SetVertexInputState
    (
        const std::span<const VkVertexInputBindingDescription> vertexBindings,
        const std::span<const VkVertexInputAttributeDescription> vertexAttribs
    )
    {
        m_vertexInputBindings      = std::vector(vertexBindings.begin(), vertexBindings.end());
        m_vertexAttribDescriptions = std::vector(vertexAttribs.begin(), vertexAttribs.end());

        m_vertexInputInfo =
        {
            .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext                           = nullptr,
            .flags                           = 0,
            .vertexBindingDescriptionCount   = static_cast<u32>(m_vertexInputBindings.size()),
            .pVertexBindingDescriptions      = m_vertexInputBindings.data(),
            .vertexAttributeDescriptionCount = static_cast<u32>(m_vertexAttribDescriptions.size()),
            .pVertexAttributeDescriptions    = m_vertexAttribDescriptions.data()
        };

        return *this;
    }

    PipelineBuilder& PipelineBuilder::SetRasterizerState(VkCullModeFlags cullMode, VkFrontFace frontFace, VkPolygonMode polygonMode)
    {
        m_rasterizationInfo =
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
        m_inputAssemblyInfo =
        {
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext                  = nullptr,
            .flags                  = 0,
            .topology               = topology,
            .primitiveRestartEnable = enablePrimitiveRestart
        };

        return *this;
    }

    PipelineBuilder& PipelineBuilder::SetMSAAState()
    {
        m_msaaStateInfo =
        {
            .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable   = VK_FALSE,
            .minSampleShading      = 0.0f,
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
        m_depthStencilInfo =
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

    PipelineBuilder& PipelineBuilder::AddBlendAttachment
    (
        VkBool32 blendEnable,
        VkBlendFactor srcColorBlendFactor,
        VkBlendFactor dstColorBlendFactor,
        VkBlendOp colorBlendOp,
        VkBlendFactor srcAlphaBlendFactor,
        VkBlendFactor dstAlphaBlendFactor,
        VkBlendOp alphaBlendOp,
        VkColorComponentFlags colorWriteMask
    )
    {
        VkPipelineColorBlendAttachmentState colorBlendAttachment =
        {
            .blendEnable         = blendEnable,
            .srcColorBlendFactor = srcColorBlendFactor,
            .dstColorBlendFactor = dstColorBlendFactor,
            .colorBlendOp        = colorBlendOp,
            .srcAlphaBlendFactor = srcAlphaBlendFactor,
            .dstAlphaBlendFactor = dstAlphaBlendFactor,
            .alphaBlendOp        = alphaBlendOp,
            .colorWriteMask      = colorWriteMask
        };

        m_colorBlendStates.emplace_back(colorBlendAttachment);

        return *this;
    }

    PipelineBuilder& PipelineBuilder::SetBlendState()
    {
        m_colorBlendInfo =
        {
            .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext           = nullptr,
            .flags           = 0,
            .logicOpEnable   = VK_FALSE,
            .logicOp         = VK_LOGIC_OP_COPY,
            .attachmentCount = static_cast<u32>(m_colorBlendStates.size()),
            .pAttachments    = m_colorBlendStates.data(),
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

        m_pushConstantRanges.emplace_back(pushConstant);

        return *this;
    }

    PipelineBuilder& PipelineBuilder::AddDescriptorLayout(VkDescriptorSetLayout layout)
    {
        m_descriptorLayouts.emplace_back(layout);

        return *this;
    }

    PipelineBuilder::~PipelineBuilder()
    {
        for (const auto& shaderModule : m_shaderModules)
        {
            shaderModule.Destroy(m_context->device);
        }
    }
}