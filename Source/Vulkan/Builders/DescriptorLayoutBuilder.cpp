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

#include "DescriptorLayoutBuilder.h"

#include "Vulkan/Util.h"
#include "Util/Log.h"

namespace Vk::Builders
{
    VkDescriptorSetLayout DescriptorLayoutBuilder::Build(VkDevice device)
    {
        const VkDescriptorSetLayoutCreateInfo createInfo =
        {
            .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext        = nullptr,
            .flags        = 0,
            .bindingCount = static_cast<u32>(bindings.size()),
            .pBindings    = bindings.data()
        };

        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        Vk::CheckResult(vkCreateDescriptorSetLayout(
            device,
            &createInfo,
            nullptr,
            &layout),
            "Failed to create descriptor layout!"
        );

        Logger::Debug("Created descriptor layout! [handle={}]\n", std::bit_cast<void*>(layout));

        return layout;
    }

    DescriptorLayoutBuilder& DescriptorLayoutBuilder::AddBinding
    (
        u32 binding,
        VkDescriptorType type,
        u32 count,
        VkShaderStageFlags shaderStages
    )
    {
        bindings.emplace_back(VkDescriptorSetLayoutBinding
        {
            .binding            = binding,
            .descriptorType     = type,
            .descriptorCount    = count,
            .stageFlags         = shaderStages,
            .pImmutableSamplers = nullptr
        });

        return *this;
    }
}