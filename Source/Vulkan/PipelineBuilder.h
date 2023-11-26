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

#ifndef PIPELINE_BUILDER_H
#define PIPELINE_BUILDER_H

#include <string_view>
#include <vector>
#include <functional>
#include <vulkan/vulkan.h>

#include "Renderer/Vertex.h"
#include "Context.h"
#include "DescriptorSetData.h"

namespace Vk
{
    class PipelineBuilder
    {
    public:
        // Pipeline data
        using PipelineData = std::tuple
        <
            VkPipeline,
            VkPipelineLayout,
            std::vector<Vk::DescriptorSetData>
        >;

        // Initialise pipeline builder
        [[nodiscard]] static PipelineBuilder Create(std::shared_ptr<Vk::Context> context, VkRenderPass renderPass);
        // Destroy pipeline data
        ~PipelineBuilder();

        // Build pipeline
        PipelineData Build();

        // Attach shader to pipeline
        [[nodiscard]] PipelineBuilder& AttachShader(const std::string_view path, VkShaderStageFlagBits shaderStage);
        // Set dynamic state objects
        [[nodiscard]] PipelineBuilder& SetDynamicStates
        (
            const std::span<const VkDynamicState> vkDynamicStates,
            const std::function<void(PipelineBuilder&)>& SetDynStates
        );
        // Set vertex input state
        [[nodiscard]] PipelineBuilder& SetVertexInputState();
        // Set IA info
        [[nodiscard]] PipelineBuilder& SetIAState();
        // Set rasterizer state
        [[nodiscard]] PipelineBuilder& SetRasterizerState(VkCullModeFlagBits cullMode, VkFrontFace frontFace);
        // Set MSAA state
        [[nodiscard]] PipelineBuilder& SetMSAAState();
        // Set color blending state
        [[nodiscard]] PipelineBuilder& SetBlendState();
        // Add push constant
        [[nodiscard]] PipelineBuilder& AddPushConstant(VkShaderStageFlags stages, u32 offset, u32 size);
        // Add descriptor set binding
        [[nodiscard]] PipelineBuilder& AddDescriptor(u32 binding, VkDescriptorType type, VkShaderStageFlags stages, usize count);

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
        // Descriptor set layout data
        std::vector<VkDescriptorSetLayoutBinding> descriptorSetBindings = {};
        // Descriptor state
        std::vector<std::pair<VkDescriptorType, usize>> descriptorStates = {};
    private:
        // Private constructor
        explicit PipelineBuilder(std::shared_ptr<Vk::Context> context, VkRenderPass renderPass);

        // Creates descriptor set layouts
        std::vector<VkDescriptorSetLayout> CreateDescriptorSetLayouts();
        // Allocate sets
        std::vector<Vk::DescriptorSetData> AllocateDescriptorSets(const std::vector<VkDescriptorSetLayout>& descriptorLayouts);

        // Vulkan context
        std::shared_ptr<Vk::Context> m_context = nullptr;
        // Render pass
        VkRenderPass m_renderPass = VK_NULL_HANDLE;
        // Vertex info
        VkVertexInputBindingDescription m_vertexInputBindings = {};
        Renderer::Vertex::VertexAttribs m_vertexAttribs  = {};
    };
}

#endif