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
#include "Vulkan/RenderPass.h"
#include "Models/Material.h"

namespace Renderer::Pipelines
{
    class ForwardPipeline
    {
    public:
        // Usings
        using MaterialMap = std::array
        <
            std::unordered_map
            <
                Models::Material,
                VkDescriptorSet,
                Models::Material::Hash,
                Models::Material::Equal
            >,
            Vk::FRAMES_IN_FLIGHT
        >;

        // Push constant info
        struct VULKAN_GLSL_DATA BasicShaderPushConstant
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
            glm::mat4 proj = {};
            // View matrix
            glm::mat4 view = {};
        };

        // Default constructor
        ForwardPipeline() = default;
        // Create pipeline
        ForwardPipeline(const std::shared_ptr<Vk::Context>& context, const Vk::RenderPass& renderPass, VkExtent2D swapchainExtent);
        // Destroy render pipeline
        void Destroy(VkDevice device);

        // Write material descriptors
        void WriteMaterialDescriptors(VkDevice device, const std::span<const Models::Material> materials);

        // Get shared UBO set data
        const Vk::DescriptorSetData& GetSceneUBOData() const;
        // Get texture sampler data
        const Vk::DescriptorSetData& GetSamplerData() const;
        // Get image data
        const Vk::DescriptorSetData& GetMaterialData() const;

        // Pipeline data
        Vk::Pipeline pipeline = {};

        // Push constant data
        std::array<BasicShaderPushConstant, Vk::FRAMES_IN_FLIGHT> pushConstants = {};
        // Shared data UBOs
        std::array<Vk::Buffer, Vk::FRAMES_IN_FLIGHT> sceneUBOs = {};
        // Texture sampler
        Vk::Sampler textureSampler = {};
        // Texture map
        MaterialMap materialMap = {};
    private:
        // Create associated pipeline data
        void CreatePipelineData(const std::shared_ptr<Vk::Context>& context);
        // Write static descriptors
        void WriteStaticDescriptors(VkDevice device);

        // Image view descriptor index
        usize imageViewDescriptorIndexOffset = 0;
    };
}

#endif
