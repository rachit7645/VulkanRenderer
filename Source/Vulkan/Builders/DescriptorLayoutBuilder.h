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

#ifndef DESCRIPTOR_LAYOUT_BUILDER_H
#define DESCRIPTOR_LAYOUT_BUILDER_H

#include <vector>
#include <vulkan/vulkan.h>

#include "Util/Util.h"

namespace Vk::Builders
{
    class DescriptorLayoutBuilder
    {
    public:
        [[nodiscard]] VkDescriptorSetLayout Build(VkDevice device);

        [[nodiscard]] DescriptorLayoutBuilder& AddBinding
        (
            u32 binding,
            VkDescriptorType type,
            u32 count,
            VkShaderStageFlags shaderStages
        );

        // Layout
        std::vector<VkDescriptorSetLayoutBinding> bindings = {};
    };
}

#endif