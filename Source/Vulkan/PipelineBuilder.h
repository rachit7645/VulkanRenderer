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
        [[nodiscard]] static PipelineBuilder Create(VkDevice device, VkRenderPass renderPass);
        // Destroy pipeline data
        ~PipelineBuilder();

        // Build pipeline
        std::tuple<VkPipeline, VkPipelineLayout> Build();

        // Attach shader to pipeline
        [[nodiscard]] PipelineBuilder& AttachShader(const std::string_view path, VkShaderStageFlagBits shaderStage);

        // Set dynamic state objects
        [[nodiscard]] PipelineBuilder& SetDynamicStates
        (
            const std::vector<VkDynamicState>& vkDynamicStates,
            const std::function<void(PipelineBuilder&)>& SetDynStates
        );

        // Set vertex input state
        [[nodiscard]] PipelineBuilder& SetVertexInputState();
        // Set IA info
        [[nodiscard]] PipelineBuilder& SetIAState();
        // Set rasterizer state
        [[nodiscard]] PipelineBuilder& SetRasterizerState(VkCullModeFlagBits vkCullMode);
        // Set MSAA state
        [[nodiscard]] PipelineBuilder& SetMSAAState();
        // Set color blending state
        [[nodiscard]] PipelineBuilder& SetBlendState();

        // Shader stages
        std::vector<VkPipelineShaderStageCreateInfo> shaderStageCreateInfos = {};

        // Dynamic states
        std::vector<VkDynamicState> dynamicStates = {};
        // Dynamic states info
        VkPipelineDynamicStateCreateInfo dynamicStateInfo = {};

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
        explicit PipelineBuilder(VkDevice device, VkRenderPass renderPass);

        // Pipeline device
        VkDevice m_device = {};
        // Render pass
        VkRenderPass m_renderPass = {};
    };
}

#endif