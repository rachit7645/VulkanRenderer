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
#include <memory>
#include <vulkan/vulkan.h>

#include "Vulkan/Context.h"
#include "IPipeline.h"

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
        void Create(const std::shared_ptr<Vk::Context>& vkContext) override;
        // Destroy render pipeline
        void Destroy(const std::shared_ptr<Vk::Context>& vkContext) override;

        // Descriptor layout
        VkDescriptorSetLayout descriptorLayout = {};
        // Shared UBOs descriptor sets
        std::array<VkDescriptorSet, Vk::FRAMES_IN_FLIGHT> sharedUBOSets = {};

        // Shared data UBOs
        std::array<Vk::Buffer, Vk::FRAMES_IN_FLIGHT> sharedUBOs = {};
        // Push constant data
        std::array<BasicShaderPushConstant, Vk::FRAMES_IN_FLIGHT> pushConstants = {};
    };
}

#endif
