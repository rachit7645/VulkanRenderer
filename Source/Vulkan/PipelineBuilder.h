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

#include "Context.h"
#include "ShaderModule.h"

namespace Vk
{
    class PipelineBuilder
    {
    public:
        using Products = std::tuple<VkPipeline, VkPipelineLayout, VkPipelineBindPoint>;

        explicit PipelineBuilder(const Vk::Context& context);
        ~PipelineBuilder();

        // No copying
        PipelineBuilder(const PipelineBuilder&) = delete;
        PipelineBuilder& operator=(const PipelineBuilder&) = delete;

        // Only moving
        PipelineBuilder(PipelineBuilder&&) = default;
        PipelineBuilder& operator=(PipelineBuilder&&) = default;

        Products Build();

        [[nodiscard]] PipelineBuilder& SetPipelineType(VkPipelineBindPoint bindPoint);

        [[nodiscard]] PipelineBuilder& SetRenderingInfo
        (
            u32 viewMask,
            const std::span<const VkFormat> colorFormats,
            VkFormat depthFormat
        );

        [[nodiscard]] PipelineBuilder& AttachShader(const std::string_view path, VkShaderStageFlagBits shaderStage);

        [[nodiscard]] PipelineBuilder& AttachShaderGroup
        (
            VkRayTracingShaderGroupTypeKHR groupType,
            u32 generalShader,
            u32 closestHitShader,
            u32 anyHitShader
        );

        [[nodiscard]] PipelineBuilder& SetMaxRayRecursionDepth(u32 maxRayRecursionDepth);

        [[nodiscard]] PipelineBuilder& SetDynamicStates(const std::span<const VkDynamicState> dynamicStates);

        [[nodiscard]] PipelineBuilder& SetIAState(VkPrimitiveTopology topology);

        [[nodiscard]] PipelineBuilder& SetRasterizerState
        (
            VkBool32 depthClampEnable,
            VkCullModeFlags cullMode,
            VkFrontFace frontFace,
            VkPolygonMode polygonMode
        );

        [[nodiscard]] PipelineBuilder& SetDepthStencilState
        (
            VkBool32 depthTestEnable,
            VkBool32 depthWriteEnable,
            VkCompareOp depthCompareOp
        );

        [[nodiscard]] PipelineBuilder& AddBlendAttachment
        (
            VkBool32 blendEnable,
            VkBlendFactor srcColorBlendFactor,
            VkBlendFactor dstColorBlendFactor,
            VkBlendOp colorBlendOp,
            VkBlendFactor srcAlphaBlendFactor,
            VkBlendFactor dstAlphaBlendFactor,
            VkBlendOp alphaBlendOp,
            VkColorComponentFlags colorWriteMask
        );

        [[nodiscard]] PipelineBuilder& AddPushConstant(VkShaderStageFlags stages, u32 offset, u32 size);
        [[nodiscard]] PipelineBuilder& AddDescriptorLayout(VkDescriptorSetLayout layout);
    private:
        void Validate();

        VkPipelineBindPoint m_pipelineType = VK_PIPELINE_BIND_POINT_GRAPHICS;

        VkPipelineRenderingCreateInfo m_renderingCreateInfo   = {};
        std::vector<VkFormat>         m_renderingColorFormats = {};

        std::vector<Vk::ShaderModule>                     m_shaderModules          = {};
        std::vector<VkPipelineShaderStageCreateInfo>      m_shaderStageCreateInfos = {};
        std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_shaderGroups           = {};

        u32 m_maxRayRecursionDepth = 0;

        std::vector<VkDynamicState>      m_dynamicStates    = {};
        VkPipelineDynamicStateCreateInfo m_dynamicStateInfo = {};

        VkPipelineViewportStateCreateInfo    m_viewportInfo    = {};
        VkPipelineVertexInputStateCreateInfo m_vertexInputInfo = {};

        VkPipelineInputAssemblyStateCreateInfo m_inputAssemblyInfo = {};
        VkPipelineRasterizationStateCreateInfo m_rasterizationInfo = {};
        VkPipelineMultisampleStateCreateInfo   m_msaaStateInfo     = {};

        VkPipelineDepthStencilStateCreateInfo m_depthStencilInfo = {};

        std::vector<VkPipelineColorBlendAttachmentState> m_colorBlendStates = {};
        VkPipelineColorBlendStateCreateInfo              m_colorBlendInfo   = {};

        std::vector<VkPushConstantRange>   m_pushConstantRanges = {};
        std::vector<VkDescriptorSetLayout> m_descriptorLayouts  = {};

        // We only need this pointer here temporarily
        const Context* m_context = nullptr;
    };
}

#endif