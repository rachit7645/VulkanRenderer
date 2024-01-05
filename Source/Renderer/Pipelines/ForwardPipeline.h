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
#include "Models/Material.h"
#include "Renderer/DirLight.h"

namespace Renderer::Pipelines
{
    class ForwardPipeline : public Vk::Pipeline
    {
    public:
        // Usings
        using MaterialMap = std::array<std::unordered_map<Models::Material, VkDescriptorSet>, Vk::FRAMES_IN_FLIGHT>;

        struct VULKAN_GLSL_DATA VSPushConstant
        {
            glm::mat4 transform    = {};
            glm::mat4 normalMatrix = {};
        };

        struct VULKAN_GLSL_DATA SceneBuffer
        {
            // Projection matrix
            glm::mat4 projection = {};
            // View matrix
            glm::mat4 view = {};
            // Camera position
            glm::vec4 cameraPos = {};
            // Light
            Renderer::DirLight dirLight = {};
        };

        ForwardPipeline() = default;

        ForwardPipeline
        (
            const std::shared_ptr<Vk::Context>& context,
            VkFormat colorFormat,
            VkFormat depthFormat,
            VkExtent2D extent
        );

        void WriteMaterialDescriptors(VkDevice device, const std::span<const Models::Material> materials);

        const Vk::DescriptorSetData& GetSceneUBOData() const;
        const Vk::DescriptorSetData& GetSamplerData()  const;
        const Vk::DescriptorSetData& GetMaterialData() const;

        std::array<VSPushConstant, Vk::FRAMES_IN_FLIGHT> pushConstants = {};
        std::array<Vk::Buffer,     Vk::FRAMES_IN_FLIGHT> sceneUBOs     = {};

        Vk::Sampler textureSampler = {};
        MaterialMap materialMap    = {};
    private:
        [[nodiscard]] Vk::Pipeline CreatePipeline
        (
            const std::shared_ptr<Vk::Context>& context,
            VkFormat colorFormat,
            VkFormat depthFormat,
            VkExtent2D extent
        );

        void CreatePipelineData(const std::shared_ptr<Vk::Context>& context);
        void DestroyPipelineData(VkDevice device, VmaAllocator allocator) override;

        void WriteStaticDescriptors(VkDevice device);

        usize textureDescriptorIndexOffset = 0;
    };
}

#endif
