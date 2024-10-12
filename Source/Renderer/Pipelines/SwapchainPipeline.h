/*
 *    Copyright 2023 - 2024 Rachit Khandelwal
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef SWAP_PIPELINE_H
#define SWAP_PIPELINE_H

#include "Util/Util.h"
#include "Vulkan/Sampler.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/Swapchain.h"
#include "Vulkan/VertexBuffer.h"
#include "Vulkan/DescriptorCache.h"
#include "Vulkan/Context.h"

namespace Renderer::Pipelines
{
    class SwapchainPipeline : public Vk::Pipeline
    {
    public:
        SwapchainPipeline(Vk::Context& context, VkFormat colorFormat);

        void WriteImageDescriptors
        (
            VkDevice device,
            Vk::DescriptorCache& descriptorCache,
            const Vk::ImageView& imageView
        ) const;

        [[nodiscard]] const std::array<Vk::DescriptorSet, Vk::FRAMES_IN_FLIGHT>& GetImageSets(Vk::DescriptorCache& descriptorCache) const;

        Vk::Sampler colorSampler;
    private:
        void CreatePipeline(Vk::Context& context, VkFormat colorFormat);
        void CreatePipelineData(const Vk::Context& context);
    };
}

#endif
