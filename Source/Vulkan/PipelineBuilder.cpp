#include "PipelineBuilder.h"
#include "ShaderModule.h"
#include "Util/Log.h"
#include "Renderer/Vertex.h"

// Usings
using Renderer::Vertex;

namespace Vk
{
    // TODO: Make everything more customisable

    PipelineBuilder PipelineBuilder::Create(VkDevice device, VkRenderPass renderPass)
    {
        // Create
        return PipelineBuilder(device, renderPass);
    }

    PipelineBuilder::PipelineBuilder(VkDevice device, VkRenderPass renderPass)
        : m_device(device),
          m_renderPass(renderPass),
          m_bindings(Vertex::GetBindingDescription()),
          m_attribs(Vertex::GetVertexAttribDescription())
    {
    }

    std::tuple<VkPipeline, VkPipelineLayout> PipelineBuilder::Build()
    {
        // Pipeline layout create info
        VkPipelineLayoutCreateInfo pipelineLayoutInfo =
        {
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext                  = nullptr,
            .flags                  = 0,
            .setLayoutCount         = 0,
            .pSetLayouts            = nullptr,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges    = nullptr
        };

        // Pipeline layout handle
        VkPipelineLayout pipelineLayout = {};

        // Create pipeline layout
        if (vkCreatePipelineLayout(
                m_device,
                &pipelineLayoutInfo,
                nullptr,
                &pipelineLayout
            ) != VK_SUCCESS)
        {
            // Log
            LOG_ERROR("Failed to create pipeline layout! [device={}]\n", reinterpret_cast<void*>(m_device));
        }

        // Log
        LOG_INFO("Created pipeline layout! [handle={}]\n", reinterpret_cast<void*>(pipelineLayout));

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
            .pDepthStencilState  = nullptr,
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
                m_device,
                nullptr,
                1,
                &pipelineCreateInfo,
                nullptr,
                &pipeline
            ) != VK_SUCCESS)
        {
            // Log
            LOG_ERROR("Failed to create pipeline! [device={}]\n", reinterpret_cast<void*>(m_device));
        }

        // Log
        LOG_INFO("Created pipeline object! [handle={}]\n", reinterpret_cast<void*>(pipeline));

        // Return
        return {pipeline, pipelineLayout};
    }

    PipelineBuilder& PipelineBuilder::AttachShader(const std::string_view path, VkShaderStageFlagBits shaderStage)
    {
        // Load shader module
        auto shaderModule = Vk::CreateShaderModule(m_device, path);

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
        const std::vector<VkDynamicState>& vkDynamicStates,
        const std::function<void(PipelineBuilder&)>& SetDynStates
    )
    {
        // Set dynamic states
        dynamicStates = vkDynamicStates;

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
            .pVertexBindingDescriptions      = &m_bindings,
            .vertexAttributeDescriptionCount = static_cast<u32>(m_attribs.size()),
            .pVertexAttributeDescriptions    = m_attribs.data()
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

    PipelineBuilder& PipelineBuilder::SetRasterizerState(VkCullModeFlagBits vkCullMode)
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
            .cullMode                = vkCullMode,
            .frontFace               = VK_FRONT_FACE_CLOCKWISE,
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

    PipelineBuilder::~PipelineBuilder()
    {
        // Loop over all shader stages
        for (auto&& stage : shaderStageCreateInfos)
        {
            // Log
            LOG_INFO("Destroying shader module [handle={}]\n", reinterpret_cast<void*>(stage.module));
            // Destroy
            vkDestroyShaderModule(m_device, stage.module, nullptr);
        }
    }
}