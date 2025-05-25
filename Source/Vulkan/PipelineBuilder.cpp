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

namespace Vk
{
    PipelineBuilder::Products PipelineBuilder::Build()
    {
        Validate();

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
                .pVertexInputState   = &m_vertexInputInfo,
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

            break;
        }

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

            break;
        }

        case VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR:
        {
            const VkRayTracingPipelineCreateInfoKHR pipelineCreateInfo =
            {
                .sType                        = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
                .pNext                        = nullptr,
                .flags                        = 0,
                .stageCount                   = static_cast<u32>(m_shaderStageCreateInfos.size()),
                .pStages                      = m_shaderStageCreateInfos.data(),
                .groupCount                   = static_cast<u32>(m_shaderGroups.size()),
                .pGroups                      = m_shaderGroups.data(),
                .maxPipelineRayRecursionDepth = m_maxRayRecursionDepth,
                .pLibraryInfo                 = nullptr,
                .pLibraryInterface            = nullptr,
                .pDynamicState                = &m_dynamicStateInfo,
                .layout                       = pipelineLayout,
                .basePipelineHandle           = VK_NULL_HANDLE,
                .basePipelineIndex            = -1
            };

            Vk::CheckResult(vkCreateRayTracingPipelinesKHR(
                m_context->device,
                VK_NULL_HANDLE,
                VK_NULL_HANDLE,
                1,
                &pipelineCreateInfo,
                nullptr,
                &pipeline),
                "Failed to create ray tracing pipeline!"
            );

            break;
        }

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
    }

    PipelineBuilder& PipelineBuilder::SetRenderingInfo
    (
        u32 viewMask,
        const std::span<const VkFormat> colorFormats,
        VkFormat depthFormat
    )
    {
        m_renderingColorFormats = std::vector(colorFormats.begin(), colorFormats.end());

        m_renderingCreateInfo =
        {
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .pNext                   = nullptr,
            .viewMask                = viewMask,
            .colorAttachmentCount    = static_cast<u32>(m_renderingColorFormats.size()),
            .pColorAttachmentFormats = m_renderingColorFormats.data(),
            .depthAttachmentFormat   = depthFormat,
            .stencilAttachmentFormat = VK_FORMAT_UNDEFINED
        };

        return *this;
    }

    PipelineBuilder& PipelineBuilder::AttachShader(const std::string_view path, VkShaderStageFlagBits shaderStage)
    {
        const auto& module = m_shaderModules.emplace_back(m_context->device, path);

        m_shaderStageCreateInfos.emplace_back(VkPipelineShaderStageCreateInfo{
            .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext               = nullptr,
            .flags               = 0,
            .stage               = shaderStage,
            .module              = module.handle,
            .pName               = "main",
            .pSpecializationInfo = nullptr
        });

        return *this;
    }

    PipelineBuilder& PipelineBuilder::AttachShaderGroup
    (
        VkRayTracingShaderGroupTypeKHR groupType,
        u32 generalShader,
        u32 closestHitShader,
        u32 anyHitShader
    )
    {
        m_shaderGroups.emplace_back(VkRayTracingShaderGroupCreateInfoKHR{
            .sType                           = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext                           = nullptr,
            .type                            = groupType,
            .generalShader                   = generalShader,
            .closestHitShader                = closestHitShader,
            .anyHitShader                    = anyHitShader,
            .intersectionShader              = VK_SHADER_UNUSED_KHR,
            .pShaderGroupCaptureReplayHandle = nullptr
        });

        return *this;
    }

    [[nodiscard]] PipelineBuilder& PipelineBuilder::SetMaxRayRecursionDepth(u32 maxRayRecursionDepth)
    {
        m_maxRayRecursionDepth = maxRayRecursionDepth;

        return *this;
    }

    PipelineBuilder& PipelineBuilder::SetDynamicStates(const std::span<const VkDynamicState> dynamicStates)
    {
        m_dynamicStates = std::vector(dynamicStates.begin(), dynamicStates.end());

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

    PipelineBuilder& PipelineBuilder::SetRasterizerState
    (
        VkBool32 depthClampEnable,
        VkCullModeFlags cullMode,
        VkFrontFace frontFace,
        VkPolygonMode polygonMode
    )
    {
        m_rasterizationInfo =
        {
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .depthClampEnable        = depthClampEnable,
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

    PipelineBuilder& PipelineBuilder::SetIAState(VkPrimitiveTopology topology)
    {
        m_inputAssemblyInfo =
        {
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext                  = nullptr,
            .flags                  = 0,
            .topology               = topology,
            .primitiveRestartEnable = VK_FALSE
        };

        return *this;
    }

    PipelineBuilder& PipelineBuilder::SetDepthStencilState
    (
        VkBool32 depthTestEnable,
        VkBool32 depthWriteEnable,
        VkCompareOp depthCompareOp
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
            .stencilTestEnable     = VK_FALSE,
            .front                 = {
                .failOp      = VK_STENCIL_OP_KEEP,
                .passOp      = VK_STENCIL_OP_KEEP,
                .depthFailOp = VK_STENCIL_OP_KEEP,
                .compareOp   = VK_COMPARE_OP_NEVER,
                .compareMask = 0,
                .writeMask   = 0,
                .reference   = 0
            },
            .back                  = {
                .failOp      = VK_STENCIL_OP_KEEP,
                .passOp      = VK_STENCIL_OP_KEEP,
                .depthFailOp = VK_STENCIL_OP_KEEP,
                .compareOp   = VK_COMPARE_OP_NEVER,
                .compareMask = 0,
                .writeMask   = 0,
                .reference   = 0
            },
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
        m_colorBlendStates.emplace_back(VkPipelineColorBlendAttachmentState{
            .blendEnable         = blendEnable,
            .srcColorBlendFactor = srcColorBlendFactor,
            .dstColorBlendFactor = dstColorBlendFactor,
            .colorBlendOp        = colorBlendOp,
            .srcAlphaBlendFactor = srcAlphaBlendFactor,
            .dstAlphaBlendFactor = dstAlphaBlendFactor,
            .alphaBlendOp        = alphaBlendOp,
            .colorWriteMask      = colorWriteMask
        });

        return *this;
    }

    PipelineBuilder& PipelineBuilder::AddPushConstant(VkShaderStageFlags stages, u32 offset, u32 size)
    {
        m_pushConstantRanges.emplace_back(VkPushConstantRange{
            .stageFlags = stages,
            .offset     = offset,
            .size       = size
        });

        return *this;
    }

    PipelineBuilder& PipelineBuilder::AddDescriptorLayout(VkDescriptorSetLayout layout)
    {
        m_descriptorLayouts.emplace_back(layout);

        return *this;
    }

    void PipelineBuilder::Validate()
    {
        // We defer actual error checking to the VVL
        // Here we just want to fill in unused but required structs / structs that can be filled with existing information

        if (m_pipelineType == VK_PIPELINE_BIND_POINT_GRAPHICS)
        {
            m_viewportInfo =
            {
                .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                .pNext         = nullptr,
                .flags         = 0,
                .viewportCount = 0,
                .pViewports    = nullptr,
                .scissorCount  = 0,
                .pScissors     = nullptr
            };

            m_vertexInputInfo =
            {
                .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .pNext                           = nullptr,
                .flags                           = 0,
                .vertexBindingDescriptionCount   = 0,
                .pVertexBindingDescriptions      = nullptr,
                .vertexAttributeDescriptionCount = 0,
                .pVertexAttributeDescriptions    = nullptr
            };

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
        }

        if (m_pipelineType == VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR)
        {
            m_dynamicStateInfo =
            {
                .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                .pNext             = nullptr,
                .flags             = 0,
                .dynamicStateCount = 0,
                .pDynamicStates    = nullptr
            };
        }
    }

    PipelineBuilder::~PipelineBuilder()
    {
        for (const auto& shaderModule : m_shaderModules)
        {
            shaderModule.Destroy(m_context->device);
        }
    }
}