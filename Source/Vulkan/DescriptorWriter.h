/*
 * Copyright (c) 2023 - 2025 Rachit
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef DESCRIPTOR_WRITER_H
#define DESCRIPTOR_WRITER_H

#include <list>
#include <vector>
#include <vulkan/vulkan.h>

#include "Util/Types.h"

namespace Vk
{
    class DescriptorWriter
    {
    public:
        DescriptorWriter& WriteImage
        (
            VkDescriptorSet set,
            u32 binding,
            u32 dstArrayElement,
            VkSampler sampler,
            VkImageView image,
            VkImageLayout layout,
            VkDescriptorType type
        );

        DescriptorWriter& WriteBuffer
        (
            VkDescriptorSet set,
            u32 binding,
            u32 dstArrayElement,
            VkBuffer buffer,
            VkDeviceSize size,
            VkDeviceSize offset,
            VkDescriptorType type
        );

        void Update(VkDevice device);

        DescriptorWriter& Clear();
    private:
        std::list<VkDescriptorImageInfo>  imageInfos;
        std::list<VkDescriptorBufferInfo> bufferInfos;
        std::vector<VkWriteDescriptorSet> writes;
    };
}

#endif
