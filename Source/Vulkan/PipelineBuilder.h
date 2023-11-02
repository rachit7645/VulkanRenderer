/*
 * Copyright 2023 Rachit Khandelwal
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

#ifndef PIPELINE_BUILDER_H
#define PIPELINE_BUILDER_H

#include <string_view>
#include <vector>
#include <functional>
#include <vulkan/vulkan.h>

#include "Renderer/Vertex.h"

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
        // Add push constant
        [[nodiscard]] PipelineBuilder& AddPushConstant(VkShaderStageFlags stages, u32 offset, u32 size);

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

        // Push constant data
        std::vector<VkPushConstantRange> pushConstantRanges = {};

    private:
        // Private constructor
        explicit PipelineBuilder(VkDevice device, VkRenderPass renderPass);

        // Pipeline device
        VkDevice m_device = {};
        // Render pass
        VkRenderPass m_renderPass = {};

        // Vertex info
        VkVertexInputBindingDescription m_bindings = {};
        Renderer::Vertex::VertexAttribs m_attribs  = {};
    };
}

#endif