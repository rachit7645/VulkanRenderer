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
#include "Vulkan/Builders/PipelineBuilder.h"
#include "Vulkan/DebugUtils.h"

namespace Renderer::SpotShadow
{
    Pipeline::Pipeline(const Vk::Context& context, const Vk::FormatHelper& formatHelper)
    {
        constexpr std::array DYNAMIC_STATES = {VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT, VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT};

        std::tie(handle, layout, bindPoint) = Vk::Builders::PipelineBuilder(context)
            .SetPipelineType(VK_PIPELINE_BIND_POINT_GRAPHICS)
            .SetRenderingInfo(0, {}, formatHelper.depthFormat, VK_FORMAT_UNDEFINED)
            .AttachShader("Shadows/SpotShadow.vert", VK_SHADER_STAGE_VERTEX_BIT)
            .AttachShader("Shadows/SpotShadow.frag", VK_SHADER_STAGE_FRAGMENT_BIT)
            .SetDynamicStates(DYNAMIC_STATES)
            .SetIAState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE)
            .SetRasterizerState(VK_TRUE, VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL)
            .SetMSAAState()
            .SetDepthStencilState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS, VK_FALSE, {}, {})
            .AddPushConstant(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(SpotShadow::PushConstant))
            .Build();

        Vk::SetDebugName(context.device, handle, "SpotShadowPipeline");
        Vk::SetDebugName(context.device, layout, "SpotShadowPipelineLayout");
    }
}