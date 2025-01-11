//    Copyright 2023 - 2024 Rachit Khandelwal
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.

#ifndef FORWARD_PIPELINE_H
#define FORWARD_PIPELINE_H

#include "ForwardInstanceBuffer.h"
#include "ForwardPushConstant.h"
#include "Vulkan/Buffer.h"
#include "Vulkan/Sampler.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/TextureManager.h"

namespace Renderer::Forward
{
    class ForwardPipeline : public Vk::Pipeline
    {
    public:
        ForwardPipeline
        (
            Vk::Context& context,
            const Vk::TextureManager& textureManager,
            VkFormat colorFormat,
            VkFormat depthFormat
        );

        Vk::DescriptorSet GetStaticSet(Vk::DescriptorCache& descriptorCache) const;

        PushConstant pushConstant = {};

        std::array<Vk::Buffer, Vk::FRAMES_IN_FLIGHT> sceneSSBOs = {};

        InstanceBuffer instanceBuffer;

        Vk::Sampler textureSampler;
    private:
        void CreatePipeline
        (
            Vk::Context& context,
            const Vk::TextureManager& textureManager,
            VkFormat colorFormat,
            VkFormat depthFormat
        );

        void CreatePipelineData(const Vk::Context& context);
        void WriteStaticDescriptor(VkDevice device, Vk::DescriptorCache& cache) const;
    };
}

#endif
