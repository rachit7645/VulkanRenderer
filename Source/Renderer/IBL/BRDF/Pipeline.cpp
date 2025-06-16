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
#include "Util/Log.h"
#include "Vulkan/DebugUtils.h"

namespace Renderer::IBL::BRDF
{
    Pipeline::Pipeline(const Vk::Context& context)
    {
        constexpr std::array DYNAMIC_STATES = {VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT, VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT};

        constexpr std::array COLOR_FORMATS = {VK_FORMAT_R16G16_SFLOAT};

        std::tie(handle, layout, bindPoint) = Vk::PipelineBuilder(context)
            .SetPipelineType(VK_PIPELINE_BIND_POINT_GRAPHICS)
            .SetRenderingInfo(0, COLOR_FORMATS, VK_FORMAT_UNDEFINED)
            .AttachShader("Misc/Trongle.vert", VK_SHADER_STAGE_VERTEX_BIT)
            .AttachShader("IBL/BRDF.frag",     VK_SHADER_STAGE_FRAGMENT_BIT)
            .SetDynamicStates(DYNAMIC_STATES)
            .SetIAState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetRasterizerState(VK_FALSE, VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL)
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
            .Build();

        Vk::SetDebugName(context.device, handle, "IBL/BRDF/Pipeline");
        Vk::SetDebugName(context.device, layout, "IBL/BRDF/PipelineLayout");
    }
}