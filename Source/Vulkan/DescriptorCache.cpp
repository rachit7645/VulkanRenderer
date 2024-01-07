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

#include "DescriptorCache.h"
#include "Util/Log.h"

namespace Vk
{
    // Constants
    constexpr u32 ALLOCATOR_INITIAL_SETS = 64;

    // Ratios
    constexpr std::array<DescriptorAllocator::PoolRatio, 4> ALLOCATOR_RATIOS =
    {
        DescriptorAllocator::PoolRatio{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         4},
        DescriptorAllocator::PoolRatio{VK_DESCRIPTOR_TYPE_SAMPLER,                4},
        DescriptorAllocator::PoolRatio{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
        DescriptorAllocator::PoolRatio{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          16}
    };

    DescriptorCache::DescriptorCache(VkDevice device)
        : m_allocator(device, ALLOCATOR_INITIAL_SETS, ALLOCATOR_RATIOS)
    {
    }

    VkDescriptorSetLayout DescriptorCache::AddLayout(const std::string& ID, VkDevice device, VkDescriptorSetLayout layout)
    {
        if (m_layouts.contains(ID))
        {
            vkDestroyDescriptorSetLayout(device, layout, nullptr);
            return m_layouts[ID];
        }

        m_layouts[ID] = layout;
        return layout;
    }

    VkDescriptorSetLayout DescriptorCache::GetLayout(const std::string& ID)
    {
        if (!m_layouts.contains(ID))
        {
            Logger::Error("Invalid ID! [ID={}]\n", ID);
        }

        return m_layouts[ID];
    }

    const Vk::DescriptorSet& DescriptorCache::AllocateSet
    (
        const std::string& ID,
        const std::string& layoutID,
        VkDevice device
    )
    {
        if (m_descriptorMap.contains(ID))
        {
            return m_descriptorMap[ID];
        }

        m_descriptorMap[ID] = m_allocator.Allocate(device, GetLayout(layoutID));
        return m_descriptorMap[ID];
    }

    const std::array<Vk::DescriptorSet, Vk::FRAMES_IN_FLIGHT>& DescriptorCache::AllocateSets
    (
        const std::string& ID,
        const std::string& layoutID,
        VkDevice device
    )
    {
        if (m_descriptorsPerFIFMap.contains(ID))
        {
            return m_descriptorsPerFIFMap[ID];
        }

        std::array<Vk::DescriptorSet, Vk::FRAMES_IN_FLIGHT> sets = {};

        auto layout = GetLayout(layoutID);

        for (auto&& set : sets)
        {
            set = m_allocator.Allocate(device, layout);
        }

        m_descriptorsPerFIFMap[ID] = sets;
        return m_descriptorsPerFIFMap[ID];
    }

    const Vk::DescriptorSet& DescriptorCache::GetSet(const std::string& ID)
    {
        if (!m_descriptorMap.contains(ID))
        {
            Logger::Error("Invalid ID! [ID={}]\n", ID);
        }

        return m_descriptorMap[ID];
    }

    const std::array<Vk::DescriptorSet, Vk::FRAMES_IN_FLIGHT>& DescriptorCache::GetSets(const std::string& ID)
    {
        if (!m_descriptorsPerFIFMap.contains(ID))
        {
            Logger::Error("Invalid ID! [ID={}]\n", ID);
        }

        return m_descriptorsPerFIFMap[ID];
    }

    void DescriptorCache::Destroy(VkDevice device)
    {
        m_allocator.Destroy(device);

        for (const auto& [ID, layout] : m_layouts)
        {
            Logger::Debug("Destroying descriptor layout! [ID={}] [handle={}] \n", ID, std::bit_cast<void*>(layout));
            vkDestroyDescriptorSetLayout(device, layout, nullptr);
        }
    }
}