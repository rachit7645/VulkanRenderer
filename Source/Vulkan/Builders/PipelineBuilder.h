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

#ifndef PIPELINE_BUILDER_H
#define PIPELINE_BUILDER_H

#include <string_view>
#include <vector>
#include <span>
#include <vulkan/vulkan.h>

#include "Vulkan/Context.h"
#include "Vulkan/ShaderModule.h"

namespace Vk::Builders
{
    class PipelineBuilder
    {
    public:
        using Products = std::pair<VkPipeline, VkPipelineLayout>;

        explicit PipelineBuilder(Vk::Context& context);
        ~PipelineBuilder();

        // No copying
        PipelineBuilder(const PipelineBuilder&) = delete;
        PipelineBuilder& operator=(const PipelineBuilder&) = delete;

        // Only moving
        PipelineBuilder(PipelineBuilder&&) = default;
        PipelineBuilder& operator=(PipelineBuilder&&) = default;

        Products Build();

        [[nodiscard]] PipelineBuilder& SetRenderingInfo
        (
            const std::span<const VkFormat> colorFormats,
            VkFormat depthFormat,
            VkFormat stencilFormat
        );

        [[nodiscard]] PipelineBuilder& AttachShader(const std::string_view path, VkShaderStageFlagBits shaderStage);
        [[nodiscard]] PipelineBuilder& SetDynamicStates(const std::span<const VkDynamicState> vkDynamicStates);

        [[nodiscard]] PipelineBuilder& SetVertexInputState
        (
            const std::span<const VkVertexInputBindingDescription>   vertexBindings,
            const std::span<const VkVertexInputAttributeDescription> vertexAttribs
        );

        [[nodiscard]] PipelineBuilder& SetIAState(VkPrimitiveTopology topology, VkBool32 enablePrimitiveRestart);
        [[nodiscard]] PipelineBuilder& SetRasterizerState(VkCullModeFlags cullMode, VkFrontFace frontFace, VkPolygonMode polygonMode);
        [[nodiscard]] PipelineBuilder& SetMSAAState();

        [[nodiscard]] PipelineBuilder& SetDepthStencilState
        (
            VkBool32 depthTestEnable,
            VkBool32 depthWriteEnable,
            VkCompareOp depthCompareOp,
            VkBool32 stencilTestEnable,
            const VkStencilOpState& front,
            const VkStencilOpState& back
        );

        [[nodiscard]] PipelineBuilder& SetBlendState();
        [[nodiscard]] PipelineBuilder& AddPushConstant(VkShaderStageFlags stages, u32 offset, u32 size);
        [[nodiscard]] PipelineBuilder& AddDescriptorLayout(VkDescriptorSetLayout layout);

        // Dynamic rendering info
        VkPipelineRenderingCreateInfo renderingCreateInfo   = {};
        std::vector<VkFormat>         renderingColorFormats;

        // Shaders
        std::vector<Vk::ShaderModule>                shaderModules;
        std::vector<VkPipelineShaderStageCreateInfo> shaderStageCreateInfos;

        // Dynamic states
        std::vector<VkDynamicState>      dynamicStates;
        VkPipelineDynamicStateCreateInfo dynamicStateInfo = {};

        // Viewport state (WHY DO I STILL NEED THIS LMAO)
        VkPipelineViewportStateCreateInfo viewportInfo = {};

        // Vertex info
        VkPipelineVertexInputStateCreateInfo           vertexInputInfo = {};
        std::vector<VkVertexInputBindingDescription>   vertexInputBindings;
        std::vector<VkVertexInputAttributeDescription> vertexAttribDescriptions;

        // Input assembly state
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {};
        // Rasterizer state info
        VkPipelineRasterizationStateCreateInfo rasterizationInfo = {};
        // MSAA state
        VkPipelineMultisampleStateCreateInfo msaaStateInfo = {};

        // Depth/Stencil State
        VkPipelineDepthStencilStateCreateInfo depthStencilInfo = {};

        // Blend info
        std::vector<VkPipelineColorBlendAttachmentState> colorBlendStates;
        VkPipelineColorBlendStateCreateInfo              colorBlendInfo   = {};

        // Push constant data
        std::vector<VkPushConstantRange> pushConstantRanges;
        // Descriptor states
        std::vector<VkDescriptorSetLayout> descriptorLayouts;
    private:
        // We only need this pointer here temporarily
        Vk::Context* m_context = nullptr;
    };
}

#endif