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

#include "PipelineConfig.h"

#include "Util/Log.h"
#include "Vulkan/Util.h"

namespace Vk
{
    void PipelineConfig::Build(VkDevice device)
    {
        for (const auto& shader : m_shaders)
        {
            const auto& module = m_shaderModules.emplace_back(device, shader.path);

            m_shaderStageCreateInfos.emplace_back(VkPipelineShaderStageCreateInfo{
                .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext               = nullptr,
                .flags               = 0,
                .stage               = shader.stage,
                .module              = module.handle,
                .pName               = "main",
                .pSpecializationInfo = nullptr
            });
        }

        switch (m_pipelineType)
        {
        case VK_PIPELINE_BIND_POINT_GRAPHICS:
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

            m_dynamicStateInfo =
            {
                .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                .pNext             = nullptr,
                .flags             = 0,
                .dynamicStateCount = static_cast<u32>(m_dynamicStates.size()),
                .pDynamicStates    = m_dynamicStates.data()
            };

            m_renderingCreateInfo =
            {
                .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                .pNext                   = nullptr,
                .viewMask                = m_renderingViewMask,
                .colorAttachmentCount    = static_cast<u32>(m_renderingColorFormats.size()),
                .pColorAttachmentFormats = m_renderingColorFormats.data(),
                .depthAttachmentFormat   = m_renderingDepthFormat,
                .stencilAttachmentFormat = VK_FORMAT_UNDEFINED
            };

            break;
        }

        case VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR:
        {
            m_dynamicStateInfo =
            {
                .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                .pNext             = nullptr,
                .flags             = 0,
                .dynamicStateCount = 0,
                .pDynamicStates    = nullptr
            };

            break;
        }

        default:
            break;
        }
    }

    VkPipelineLayout PipelineConfig::BuildLayout(VkDevice device)
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
            device,
            &pipelineLayoutInfo,
            nullptr,
            &pipelineLayout),
            "Failed to create pipeline layout!"
        );

        return pipelineLayout;
    }

    VkGraphicsPipelineCreateInfo PipelineConfig::BuildGraphicsPipelineCreateInfo(VkPipelineLayout pipelineLayout)
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

        return pipelineCreateInfo;
    }

    VkComputePipelineCreateInfo PipelineConfig::BuildComputePipelineCreateInfo(VkPipelineLayout pipelineLayout)
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

        return pipelineCreateInfo;
    }

    VkRayTracingPipelineCreateInfoKHR PipelineConfig::BuildRayTracingPipelineCreateInfo(VkPipelineLayout pipelineLayout)
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

        return pipelineCreateInfo;
    }

    PipelineConfig& PipelineConfig::SetPipelineType(VkPipelineBindPoint bindPoint)
    {
        m_pipelineType = bindPoint;

        return *this;
    }

    VkPipelineBindPoint PipelineConfig::GetPipelineType() const
    {
        return m_pipelineType;
    }

    PipelineConfig& PipelineConfig::SetRenderingInfo
    (
        u32 viewMask,
        const std::span<const VkFormat> colorFormats,
        VkFormat depthFormat
    )
    {
        m_renderingColorFormats = std::vector(colorFormats.begin(), colorFormats.end());
        m_renderingDepthFormat  = depthFormat;
        m_renderingViewMask     = viewMask;

        return *this;
    }

    PipelineConfig& PipelineConfig::AttachShader(const std::string_view path, VkShaderStageFlagBits shaderStage)
    {
        m_shaders.emplace_back(Vk::PipelineShader{
            .path  = path.data(),
            .stage = shaderStage
        });

        return *this;
    }

    PipelineConfig& PipelineConfig::AttachShaderGroup
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

    [[nodiscard]] PipelineConfig& PipelineConfig::SetMaxRayRecursionDepth(u32 maxRayRecursionDepth)
    {
        m_maxRayRecursionDepth = maxRayRecursionDepth;

        return *this;
    }

    PipelineConfig& PipelineConfig::SetDynamicStates(const std::span<const VkDynamicState> dynamicStates)
    {
        m_dynamicStates = std::vector(dynamicStates.begin(), dynamicStates.end());

        return *this;
    }

    PipelineConfig& PipelineConfig::SetRasterizerState
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

    PipelineConfig& PipelineConfig::SetIAState(VkPrimitiveTopology topology)
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

    PipelineConfig& PipelineConfig::SetDepthStencilState
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

    PipelineConfig& PipelineConfig::AddDefaultBlendAttachment()
    {
        return AddBlendAttachment(
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
        );
    }

    PipelineConfig& PipelineConfig::AddBlendAttachment
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

    PipelineConfig& PipelineConfig::AddPushConstant(VkShaderStageFlags stages, u32 offset, u32 size)
    {
        m_pushConstantRanges.emplace_back(VkPushConstantRange{
            .stageFlags = stages,
            .offset     = offset,
            .size       = size
        });

        return *this;
    }

    PipelineConfig& PipelineConfig::AddDescriptorLayout(VkDescriptorSetLayout layout)
    {
        m_descriptorLayouts.emplace_back(layout);

        return *this;
    }

    std::span<const PipelineShader> PipelineConfig::GetShaders() const
    {
        return m_shaders;
    }

    void PipelineConfig::Destroy(VkDevice device)
    {
        for (const auto& shaderModule : m_shaderModules)
        {
            shaderModule.Destroy(device);
        }

        m_shaderModules.clear();
        m_shaderStageCreateInfos.clear();
    }
}