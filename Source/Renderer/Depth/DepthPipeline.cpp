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

#include "DepthPipeline.h"
#include "Vulkan/Builders/PipelineBuilder.h"
#include "Vulkan/DebugUtils.h"

namespace Renderer::Depth
{
    DepthPipeline::DepthPipeline
    (
        const Vk::Context& context,
        const Vk::FormatHelper& formatHelper,
        const Vk::MegaSet& megaSet
    )
    {
        constexpr std::array DYNAMIC_STATES = {VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT, VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT};

        std::tie(handle, layout) = Vk::Builders::PipelineBuilder(context)
            .SetPipelineType(Vk::Builders::PipelineBuilder::PipelineType::Graphics)
            .SetRenderingInfo({}, formatHelper.depthFormat, VK_FORMAT_UNDEFINED)
            .AttachShader("Depth.vert.spv", VK_SHADER_STAGE_VERTEX_BIT)
            .AttachShader("Depth.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
            .SetDynamicStates(DYNAMIC_STATES)
            .SetIAState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE)
            .SetRasterizerState(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL)
            .SetMSAAState()
            .SetDepthStencilState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_GREATER, VK_FALSE, {}, {})
            .AddPushConstant(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Depth::PushConstant))
            .AddDescriptorLayout(megaSet.descriptorSet.layout)
            .Build();

        Vk::SetDebugName(context.device, handle, "DepthPipeline");
        Vk::SetDebugName(context.device, layout, "DepthPipelineLayout");
    }
}