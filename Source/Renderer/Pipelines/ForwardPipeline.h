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

        // Push constant info
        struct VULKAN_GLSL_DATA VSPushConstant
        {
            // Transformation matrix
            glm::mat4 transform = {};
            // Normal matrix
            glm::mat4 normalMatrix = {};
        };

        // Scene data UBO
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

        // Default constructor
        ForwardPipeline() = default;
        // Create pipeline
        ForwardPipeline(const std::shared_ptr<Vk::Context>& context, const Vk::RenderPass& renderPass, VkExtent2D extent);

        // Write material descriptors
        void WriteMaterialDescriptors(VkDevice device, const std::span<const Models::Material> materials);

        // Get shared UBO set data
        const Vk::DescriptorSetData& GetSceneUBOData() const;
        // Get texture sampler data
        const Vk::DescriptorSetData& GetSamplerData() const;
        // Get image data
        const Vk::DescriptorSetData& GetMaterialData() const;

        // Push constant data
        std::array<VSPushConstant, Vk::FRAMES_IN_FLIGHT> pushConstants = {};
        // Shared data UBOs
        std::array<Vk::Buffer, Vk::FRAMES_IN_FLIGHT> sceneUBOs = {};
        // Texture sampler
        Vk::Sampler textureSampler = {};
        // Texture map
        MaterialMap materialMap = {};
    private:
        // Create pipeline
        [[nodiscard]] Vk::Pipeline CreatePipeline(const std::shared_ptr<Vk::Context>& context, const Vk::RenderPass& renderPass, VkExtent2D extent);
        // Create associated pipeline data
        void CreatePipelineData(const std::shared_ptr<Vk::Context>& context);
        // Destroy per-pipeline data
        void DestroyPipelineData(VkDevice device) const override;

        // Write static descriptors
        void WriteStaticDescriptors(VkDevice device);

        // Texture descriptors index offset
        usize textureDescriptorIndexOffset = 0;
    };
}

#endif
