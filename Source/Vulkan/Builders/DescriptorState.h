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

#ifndef DESCRIPTOR_STATE_H
#define DESCRIPTOR_STATE_H

#include <vulkan/vulkan.h>
#include "Util/Util.h"

namespace Vk::Builders
{
    struct DescriptorState
    {
        u32                          count   = 0;
        VkDescriptorType             type    = {};
        VkDescriptorSetLayoutBinding binding = {};
        VkDescriptorSetLayout        layout  = {};
    };
}

#endif
