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

#ifndef POST_PROCESS_PIPELINE_H
#define POST_PROCESS_PIPELINE_H

#include "PostProcessConstants.h"
#include "Vulkan/Sampler.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/MegaSet.h"
#include "Vulkan/TextureManager.h"
#include "Vulkan/Context.h"

namespace Renderer::PostProcess
{
    class PostProcessPipeline : public Vk::Pipeline
    {
    public:
        PostProcessPipeline
        (
            const Vk::Context& context,
            Vk::MegaSet& megaSet,
            Vk::TextureManager& textureManager,
            VkFormat colorFormat
        );

        void WriteColorAttachmentIndex
        (
            VkDevice device,
            Vk::MegaSet& megaSet,
            const Vk::ImageView& imageView
        );

        PushConstant pushConstant = {};

        u32 samplerIndex         = 0;
        u32 colorAttachmentIndex = 0;
    private:
        void CreatePipeline(const Vk::Context& context, const Vk::MegaSet& megaSet, VkFormat colorFormat);
        void CreatePipelineData(VkDevice device, Vk::MegaSet& megaSet, Vk::TextureManager& textureManager);
    };
}

#endif
