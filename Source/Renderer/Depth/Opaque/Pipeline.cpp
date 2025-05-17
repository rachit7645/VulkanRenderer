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
#include "Deferred/Depth/Opaque.h"

namespace Renderer::Depth::Opaque
{
    Pipeline::Pipeline(const Vk::Context& context, const Vk::FormatHelper& formatHelper)
    {
        constexpr std::array DYNAMIC_STATES =
        {
            VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT,
            VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT,
            VK_DYNAMIC_STATE_CULL_MODE
        };

        std::tie(handle, layout, bindPoint) = Vk::PipelineBuilder(context)
            .SetPipelineType(VK_PIPELINE_BIND_POINT_GRAPHICS)
            .SetRenderingInfo(0, {}, formatHelper.depthFormat, VK_FORMAT_UNDEFINED)
            .AttachShader("Deferred/Depth/Opaque.vert", VK_SHADER_STAGE_VERTEX_BIT)
            .AttachShader("Misc/Empty.frag",             VK_SHADER_STAGE_FRAGMENT_BIT)
            .SetDynamicStates(DYNAMIC_STATES)
            .SetIAState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE)
            .SetRasterizerState(VK_FALSE, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_POLYGON_MODE_FILL)
            .SetMSAAState()
            .SetDepthStencilState(VK_TRUE, VK_TRUE, VK_COMPARE_OP_GREATER, VK_FALSE, {}, {})
            .AddPushConstant(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Opaque::Constants))
            .Build();

        Vk::SetDebugName(context.device, handle, "DepthPipeline");
        Vk::SetDebugName(context.device, layout, "DepthPipelineLayout");
    }
}