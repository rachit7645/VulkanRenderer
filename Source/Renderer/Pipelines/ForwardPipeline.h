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

#ifndef FORWARD_PIPELINE_H
#define FORWARD_PIPELINE_H

#include "Util/Util.h"
#include "Vulkan/Buffer.h"
#include "Vulkan/Sampler.h"
#include "Vulkan/Texture.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/Util.h"
#include "Models/Material.h"
#include "Renderer/DirLight.h"
#include "Externals/GLM.h"

namespace Renderer::Pipelines
{
    class ForwardPipeline : public Vk::Pipeline
    {
    public:
        // Usings
        using MaterialMap = std::unordered_map<Models::Material, std::array<Vk::DescriptorSet, Vk::FRAMES_IN_FLIGHT>>;

        struct VULKAN_GLSL_DATA PushConstant
        {
            glm::mat4       transform    = {};
            glm::mat4       normalMatrix = {};
            VkDeviceAddress scene        = {};
        };

        struct VULKAN_GLSL_DATA SceneBuffer
        {
            glm::mat4 projection = {};
            glm::mat4 view       = {};
            glm::vec4 cameraPos  = {};
            DirLight  dirLight   = {};
        };

        ForwardPipeline() = default;

        ForwardPipeline
        (
            const std::shared_ptr<Vk::Context>& context,
            VkFormat colorFormat,
            VkFormat depthFormat
        );

        void WriteMaterialDescriptors
        (
            VkDevice device,
            Vk::DescriptorCache& descriptorCache,
            const std::span<const Models::Material> materials
        );

        const std::array<Vk::DescriptorSet, Vk::FRAMES_IN_FLIGHT>& GetStaticSets(Vk::DescriptorCache& descriptorCache) const;

        PushConstant pushConstant = {};
        // Updated coherently so needs to be duplicated
        std::array<Vk::Buffer, Vk::FRAMES_IN_FLIGHT> sceneSSBOs = {};

        Vk::Sampler textureSampler = {};
        MaterialMap materialMap    = {};
    private:
        [[nodiscard]] Vk::Pipeline CreatePipeline
        (
            const std::shared_ptr<Vk::Context>& context,
            VkFormat colorFormat,
            VkFormat depthFormat
        );

        void CreatePipelineData(const std::shared_ptr<Vk::Context>& context);

        void WriteStaticDescriptors(VkDevice device, Vk::DescriptorCache& cache) const;

        usize materialDescriptorIDOffset = 0;
    };
}

#endif
