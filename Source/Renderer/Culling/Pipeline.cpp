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

namespace Renderer::Culling
{
    Pipeline::Pipeline(const Vk::Context& context)
    {
        std::tie(handle, layout, bindPoint) = Vk::Builders::PipelineBuilder(context)
            .SetPipelineType(VK_PIPELINE_BIND_POINT_COMPUTE)
            .AttachShader("Culling/Frustum.comp", VK_SHADER_STAGE_COMPUTE_BIT)
            .AddPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Culling::PushConstant))
            .Build();

        Vk::SetDebugName(context.device, handle, "CullingPipeline");
        Vk::SetDebugName(context.device, layout, "CullingPipelineLayout");
    }
}