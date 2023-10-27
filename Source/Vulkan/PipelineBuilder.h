#ifndef PIPELINE_BUILDER_H
#define PIPELINE_BUILDER_H

#include <string_view>
#include <vector>
#include <functional>
#include <vulkan/vulkan.h>

namespace Vk
{
    class PipelineBuilder
    {
    public:
        // Initialise pipeline builder
        static PipelineBuilder Create(VkDevice device);
        // Destroy pipeline data
        ~PipelineBuilder();

        // Build pipeline
        VkPipeline Build();

        // Attach shader to pipeline
        PipelineBuilder& AttachShader(const std::string_view path, VkShaderStageFlagBits shaderStage);

        // Set dynamic state objects
        PipelineBuilder& SetDynamicStates
        (
            const std::vector<VkDynamicState>& vkDynamicStates,
            const std::function<void(PipelineBuilder&)>& SetDynStates
        );

        // Set vertex input state
        PipelineBuilder& SetVertexInputState();
        // Set IA info
        PipelineBuilder& SetIAState();
        // Set rasterizer state
        PipelineBuilder& SetRasterizerState(VkCullModeFlagBits vkCullMode);
        // Set MSAA state
        PipelineBuilder& SetMSAAState();
        // Set color blending state
        PipelineBuilder& SetBlendState();

        // Create pipeline layout object
        PipelineBuilder& CreatePipelineLayout();

        // Shader stages
        std::vector<VkPipelineShaderStageCreateInfo> shaderStageCreateInfos = {};

        // Dynamic states
        std::vector<VkDynamicState> dynamicStates = {};
        // Dynamic states info
        VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};

        // Viewport state
        VkPipelineViewportStateCreateInfo viewportInfo = {};
        // Viewport data
        VkViewport viewport = {};
        // Scissor data
        VkRect2D scissor = {};

        // Vertex input state info
        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        // Input assembly state info
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {};
        // Rasterizer state info
        VkPipelineRasterizationStateCreateInfo rasterizationInfo = {};
        // MSAA state
        VkPipelineMultisampleStateCreateInfo msaaStateInfo = {};

        // Blend states
        std::vector<VkPipelineColorBlendAttachmentState> colorBlendStates = {};
        // Blend states info
        VkPipelineColorBlendStateCreateInfo colorBlendInfo = {};

    private:
        // Private constructor
        explicit PipelineBuilder(VkDevice device);

        // Pipeline device
        VkDevice m_device = {};
    };
}

#endif