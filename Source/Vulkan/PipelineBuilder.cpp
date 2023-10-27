#include "PipelineBuilder.h"
#include "ShaderModule.h"
#include "Util/Log.h"

namespace Vk
{
    PipelineBuilder PipelineBuilder::Create(VkDevice device)
    {
        // Create
        return PipelineBuilder(device);
    }

    PipelineBuilder::PipelineBuilder(VkDevice device)
        : m_device(device)
    {
    }

    VkPipeline PipelineBuilder::Build()
    {
        // We'll be right back
        return VK_NULL_HANDLE;
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
        dynamicStateCreateInfo =
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
            .vertexBindingDescriptionCount   = 0,
            .pVertexBindingDescriptions      = nullptr,
            .vertexAttributeDescriptionCount = 0,
            .pVertexAttributeDescriptions    = nullptr
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
        // Create blending state for attachments

        // Return
        return *this;
    }

    PipelineBuilder& PipelineBuilder::CreatePipelineLayout()
    {
        // Return
        return *this;
    }

    PipelineBuilder::~PipelineBuilder()
    {
        // Loop over all shader stages
        for (auto&& stage : shaderStageCreateInfos)
        {
            // Log
            LOG_DEBUG("Destroying shader module [handle={}]\n", reinterpret_cast<void*>(stage.module));
            // Destroy
            vkDestroyShaderModule(m_device, stage.module, nullptr);
        }
    }
}