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

#ifndef I_PIPELINE_H
#define I_PIPELINE_H

#include <memory>
#include <vulkan/vulkan.h>
#include "Vulkan/Context.h"

namespace Renderer
{
    struct IPipeline
    {
        // Destructor
        virtual ~IPipeline() = default;

        // Create pipeline
        virtual void Create(UNUSED const std::shared_ptr<Vk::Context>& vkContext);
        // Destroy pipeline
        virtual void Destroy(UNUSED const std::shared_ptr<Vk::Context>& vkContext);

        // Pipeline data
        VkPipeline pipeline = {};
        // Layout
        VkPipelineLayout pipelineLayout = {};
    };
}

#endif
