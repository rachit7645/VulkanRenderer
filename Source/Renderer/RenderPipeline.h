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

#include "Util/Util.h"
#include "Externals/GLM.h"
#include "Vulkan/Buffer.h"
#include "Vulkan/Sampler.h"
#include "Vulkan/DescriptorSetData.h"
#include "Vulkan/Texture.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/Swapchain.h"

namespace Renderer
{
    class RenderPipeline
    {
    public:
        // Usings
        using ImageViewMap = std::array
        <
            std::unordered_map
            <
                Vk::ImageView,
                VkDescriptorSet,
                Vk::ImageView::Hash,
                Vk::ImageView::Equal
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

        // Common data UBO
        struct VULKAN_GLSL_DATA SharedBuffer
        {
            // View matrix
            glm::mat4 view = {};
            // Projection matrix
            glm::mat4 proj = {};
        };

        // Create render pipeline
        RenderPipeline(const std::shared_ptr<Vk::Context>& vkContext, const std::shared_ptr<Vk::Swapchain>& swapchain);
        // Destroy render pipeline
        void Destroy(VkDevice device);

        // Write image descriptors
        void WriteImageDescriptors(VkDevice device, const std::vector<Vk::ImageView>& imageViews);

        // Get shared UBO set data
        const Vk::DescriptorSetData& GetSharedUBOData() const;
        // Get texture sampler data
        const Vk::DescriptorSetData& GetSamplerData() const;
        // Get image data
        const Vk::DescriptorSetData& GetImageData() const;

        // Pipeline data
        Vk::Pipeline pipeline = {};

        // Push constant data
        std::array<BasicShaderPushConstant, Vk::FRAMES_IN_FLIGHT> pushConstants = {};
        // Shared data UBOs
        std::array<Vk::Buffer, Vk::FRAMES_IN_FLIGHT> sharedUBOs = {};
        // Texture sampler
        Vk::Sampler textureSampler = {};
        // Image view map
        ImageViewMap imageViewMap = {};
    private:
        // Create associated pipeline data
        void CreatePipelineData(const std::shared_ptr<Vk::Context>& vkContext);
        // Write static descriptors
        void WriteStaticDescriptors(VkDevice device);

        // Image view descriptor index
        usize imageViewDescriptorIndexOffset = 0;
    };
}

#endif
