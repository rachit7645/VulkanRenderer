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

#include "Pipeline.h"
#include "Vulkan/PipelineBuilder.h"
#include "Vulkan/DebugUtils.h"
#include "Shadows/PointShadow/Opaque.h"

namespace Renderer::PointShadow::Opaque
{
    Pipeline::Pipeline(const Vk::Context& context, const Vk::FormatHelper& formatHelper)
    {
        constexpr std::array DYNAMIC_STATES =
        {
            VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
            VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
            VK_DYNAMIC_STATE_CULL_MODE
        };

        constexpr std::array COLOR_FORMATS = {VK_FORMAT_R32_SFLOAT};

        std::tie(handle, layout, bindPoint) = Vk::PipelineBuilder(context)
            .SetPipelineType(VK_PIPELINE_BIND_POINT_GRAPHICS)
            .SetRenderingInfo(0, COLOR_FORMATS, formatHelper.depthFormat)
            .AttachShader("Shadows/PointShadow/Opaque.vert", VK_SHADER_STAGE_VERTEX_BIT)
            .AttachShader("Shadows/PointShadow/Opaque.frag", VK_SHADER_STAGE_FRAGMENT_BIT)
            .SetDynamicStates(DYNAMIC_STATES)
            .SetIAState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetRasterizerState(VK_FALSE, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL)
            .SetDepthStencilState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_GREATER)
            .AddBlendAttachment(
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
            )
            .AddPushConstant(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Opaque::Constants))
            .Build();

        Vk::SetDebugName(context.device, handle, "PointShadow/Opaque/Pipeline");
        Vk::SetDebugName(context.device, layout, "PointShadow/Opaque/Pipeline/Layout");
    }
}