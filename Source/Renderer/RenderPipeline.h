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

#ifndef RENDER_PIPELINE_H
#define RENDER_PIPELINE_H

#include "IPipeline.h"
#include "Util/Util.h"
#include "Externals/GLM.h"
#include "Vulkan/Buffer.h"
#include "Vulkan/Sampler.h"
#include "Vulkan/DescriptorSetData.h"

namespace Renderer
{
    class RenderPipeline : public IPipeline
    {
    public:
        // Push constant info
        struct VULKAN_GLSL_DATA BasicShaderPushConstant
        {
            // Transformation matrix
            glm::mat4 model = {};
        };

        // Common data UBO
        struct VULKAN_GLSL_DATA SharedBuffer
        {
            // View matrix
            glm::mat4 view = {};
            // Projection matrix
            glm::mat4 proj = {};
        };

        // Create render pipeline
        void Create(const std::shared_ptr<Vk::Context>& vkContext, const std::shared_ptr<Vk::Swapchain>& swapchain);
        // Destroy render pipeline
        void Destroy(VkDevice device);

        // Write image descriptors
        void WriteImageDescriptors(VkDevice device, const Vk::ImageView& imageView);

        // Get shared UBO set data
        Vk::DescriptorSetData& GetSharedUBOData();
        // Get texture sampler data
        Vk::DescriptorSetData& GetTextureSamplerData();

        // Push constant data
        std::array<BasicShaderPushConstant, Vk::FRAMES_IN_FLIGHT> pushConstants = {};
        // Descriptor data
        std::vector<Vk::DescriptorSetData> descriptorData = {};

        // Shared data UBOs
        std::array<Vk::Buffer, Vk::FRAMES_IN_FLIGHT> sharedUBOs = {};
        // Texture sampler
        Vk::Sampler textureSampler = {};
    private:
        // Create associated pipeline data
        void CreatePipelineData(const std::shared_ptr<Vk::Context>& vkContext);
        // Write shared UBO descriptors
        void WriteSharedUBODescriptors(VkDevice device);
    };
}

#endif
