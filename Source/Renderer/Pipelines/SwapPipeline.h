/*
 *    Copyright 2023 Rachit Khandelwal
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
#include "Vulkan/RenderPass.h"
#include "Vulkan/VertexBuffer.h"

namespace Renderer::Pipelines
{
    class SwapPipeline
    {
    public:
        // Deafault constructor
        SwapPipeline() = default;
        // Create swapchain pipeline
        SwapPipeline(const std::shared_ptr<Vk::Context>& context, const Vk::RenderPass& swapPass, VkExtent2D swapExtent);

        // Destroy render pipeline
        void Destroy(VkDevice device);

        // Write sampled image descriptors
        void WriteImageDescriptors(VkDevice device, const std::span<Vk::ImageView, Vk::FRAMES_IN_FLIGHT> imageViews);

        // Get texture image data
        [[nodiscard]] const Vk::DescriptorSetData& GetImageData() const;

        // Pipeline data
        Vk::Pipeline pipeline = {};
        // Texture sampler
        Vk::Sampler textureSampler = {};
        // Screen quad
        Vk::VertexBuffer screenQuad = {};
    private:
        // Create associated pipeline data
        void CreatePipelineData(const std::shared_ptr<Vk::Context>& context);
    };
}

#endif