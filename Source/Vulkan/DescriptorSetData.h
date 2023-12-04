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

#ifndef DESCRIPTOR_SET_DATA_H
#define DESCRIPTOR_SET_DATA_H

#include <array>
#include <vector>
#include <vulkan/vulkan.h>

namespace Vk
{
    struct DescriptorSetData
    {
        // Usings
        using DescriptorMap = std::array<std::vector<VkDescriptorSet>, Vk::FRAMES_IN_FLIGHT>;
        // Binding
        u32 binding = 0;
        // Type
        VkDescriptorType type = {};
        // Layout
        VkDescriptorSetLayout layout = {};
        // Descriptors
        DescriptorMap setMap = {};
    };
}

#endif
