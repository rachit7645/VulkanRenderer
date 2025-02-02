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

#ifndef DEPTH_PIPELINE_H
#define DEPTH_PIPELINE_H

#include "Vulkan/Pipeline.h"
#include "Vulkan/FormatHelper.h"
#include "Vulkan/MegaSet.h"
#include "Constants.h"

namespace Renderer::RenderPass
{
    class Pipeline : public Vk::Pipeline
    {
    public:
        Pipeline
        (
            const Vk::Context& context,
            const Vk::FormatHelper& formatHelper,
            const Vk::MegaSet& megaSet
        );

        RenderPass::PushConstant pushConstant = {};
    };
}

#endif
