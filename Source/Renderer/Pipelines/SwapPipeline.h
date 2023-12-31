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

namespace Renderer::Pipelines
{
    class SwapPipeline : public Vk::Pipeline
    {
    public:
        SwapPipeline
        (
            const std::shared_ptr<Vk::Context>& context,
            VkFormat colorFormat,
            VkExtent2D extent
        );

        void WriteImageDescriptors(VkDevice device, const std::span<Vk::ImageView, Vk::FRAMES_IN_FLIGHT> imageViews);

        [[nodiscard]] const Vk::DescriptorSetData& GetImageData() const;

        Vk::Sampler      textureSampler = {};
        Vk::VertexBuffer screenQuad     = {};
    private:
        [[nodiscard]] Vk::Pipeline CreatePipeline
        (
            const std::shared_ptr<Vk::Context>& context,
            VkFormat colorFormat,
            VkExtent2D extent
        );

        void CreatePipelineData(const std::shared_ptr<Vk::Context>& context);
        void DestroyPipelineData(VkDevice device, VmaAllocator allocator) override;
    };
}

#endif
