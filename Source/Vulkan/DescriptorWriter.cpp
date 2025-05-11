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

#include "DescriptorWriter.h"

#include <volk/volk.h>

namespace Vk
{
    DescriptorWriter& DescriptorWriter::WriteImage
    (
        VkDescriptorSet set,
        u32 binding,
        u32 dstArrayElement,
        VkSampler sampler,
        VkImageView image,
        VkImageLayout layout,
        VkDescriptorType type
    )
    {
        const auto& info = imageInfos.emplace_back(VkDescriptorImageInfo
        {
            .sampler     = sampler,
            .imageView   = image,
            .imageLayout = layout
        });

        writes.emplace_back(VkWriteDescriptorSet{
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = set,
            .dstBinding       = binding,
            .dstArrayElement  = dstArrayElement,
            .descriptorCount  = 1,
            .descriptorType   = type,
            .pImageInfo       = &info,
            .pBufferInfo      = nullptr,
            .pTexelBufferView = nullptr
        });

        return *this;
    }

    DescriptorWriter& DescriptorWriter::WriteBuffer
    (
        VkDescriptorSet set,
        u32 binding,
        u32 dstArrayElement,
        VkBuffer buffer,
        VkDeviceSize size,
        VkDeviceSize offset,
        VkDescriptorType type
    )
    {
        auto& info = bufferInfos.emplace_back(VkDescriptorBufferInfo
        {
            .buffer = buffer,
            .offset = offset,
            .range  = size
        });

        writes.emplace_back(VkWriteDescriptorSet{
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext            = nullptr,
            .dstSet           = set,
            .dstBinding       = binding,
            .dstArrayElement  = dstArrayElement,
            .descriptorCount  = 1,
            .descriptorType   = type,
            .pImageInfo       = nullptr,
            .pBufferInfo      = &info,
            .pTexelBufferView = nullptr
        });

        return *this;
    }

    void DescriptorWriter::Update(VkDevice device)
    {
        if (writes.empty())
        {
            return;
        }

        vkUpdateDescriptorSets
        (
            device,
            static_cast<u32>(writes.size()),
            writes.data(),
            0,
            nullptr
        );

        Clear();
    }

    DescriptorWriter& DescriptorWriter::Clear()
    {
        imageInfos.clear();
        bufferInfos.clear();
        writes.clear();

        return *this;
    }
}