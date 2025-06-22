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
#include "Shadows/ShadowRT.h"

namespace Renderer::ShadowRT
{
    Pipeline::Pipeline(const Vk::Context& context, const Vk::MegaSet& megaSet)
    {
        std::tie(handle, layout, bindPoint) = Vk::PipelineBuilder(context)
            .SetPipelineType(VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR)
            .AttachShader("Shadows/RT/Shadow.rgen",  VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .AttachShader("Shadows/RT/Shadow.rmiss", VK_SHADER_STAGE_MISS_BIT_KHR)
            .AttachShader("Shadows/RT/Shadow.rahit", VK_SHADER_STAGE_ANY_HIT_BIT_KHR)
            .AttachShaderGroup(VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,             0,                    VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR)
            .AttachShaderGroup(VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,             1,                    VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR)
            .AttachShaderGroup(VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, 2                   )
            .SetMaxRayRecursionDepth(1)
            .AddPushConstant(VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, 0, sizeof(ShadowRT::Constants))
            .AddDescriptorLayout(megaSet.descriptorLayout)
            .Build();

        Vk::SetDebugName(context.device, handle, "ShadowRT/Pipeline");
        Vk::SetDebugName(context.device, layout, "ShadowRT/Pipeline/Layout");
    }
}