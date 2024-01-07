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

#include "DescriptorAllocator.h"
#include "Util.h"

namespace Vk
{
    // Constants
    constexpr f32 GROW_FACTOR       = 1.5f;
    constexpr u32 MAX_SETS_PER_POOL = 4096;

    DescriptorAllocator::DescriptorAllocator(VkDevice device, u32 initialSetCount, const std::span<const PoolRatio> ratios)
        : m_ratios(ratios.begin(), ratios.end()),
          m_setsPerPool(static_cast<u32>(GROW_FACTOR * static_cast<f32>(initialSetCount)))
    {
        m_readyPools.emplace_back(CreatePool(device, initialSetCount, ratios));
    }

    Vk::DescriptorSet DescriptorAllocator::Allocate(VkDevice device, VkDescriptorSetLayout layout)
    {
        VkDescriptorPool poolToUse = GetPool(device);

        VkDescriptorSetAllocateInfo allocInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext              = nullptr,
            .descriptorPool     = poolToUse,
            .descriptorSetCount = 1,
            .pSetLayouts        = &layout
        };

        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);

        if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL)
        {
            m_fullPools.push_back(poolToUse);
            poolToUse = GetPool(device);
            allocInfo.descriptorPool = poolToUse;

            Vk::CheckResult(vkAllocateDescriptorSets(
                device,
                &allocInfo,
                &descriptorSet),
                "Failed to allocate descriptor set!"
            );
        }

        m_readyPools.push_back(poolToUse);

        return {descriptorSet, layout};
    }

    void DescriptorAllocator::Clear(VkDevice device)
    {
        for (auto pool : m_readyPools)
        {
            vkResetDescriptorPool(device, pool, 0);
        }

        for (auto pool : m_fullPools)
        {
            vkResetDescriptorPool(device, pool, 0);
            m_readyPools.push_back(pool);
        }

        m_fullPools.clear();
    }

    void DescriptorAllocator::Destroy(VkDevice device)
    {
        for (auto pool : m_readyPools)
        {
            vkDestroyDescriptorPool(device, pool, nullptr);
        }

        for (auto pool : m_fullPools)
        {
            vkDestroyDescriptorPool(device,pool,nullptr);
        }

        m_readyPools.clear();
        m_fullPools.clear();
    }

    VkDescriptorPool DescriptorAllocator::GetPool(VkDevice device)
    {
        VkDescriptorPool pool = VK_NULL_HANDLE;

        if (!m_readyPools.empty())
        {
            pool = m_readyPools.back();
            m_readyPools.pop_back();
        }
        else
        {
            pool = CreatePool(device, m_setsPerPool, m_ratios);

            m_setsPerPool = static_cast<u32>(GROW_FACTOR * static_cast<f32>(m_setsPerPool));
            m_setsPerPool = std::min(m_setsPerPool, MAX_SETS_PER_POOL);
        }

        return pool;
    }

    VkDescriptorPool DescriptorAllocator::CreatePool(VkDevice device, u32 setCount, const std::span<const PoolRatio> ratios)
    {
        // Pre-allocate
        std::vector<VkDescriptorPoolSize> poolSizes;
        poolSizes.reserve(ratios.size());

        for (const auto& ratio : ratios)
        {
            poolSizes.emplace_back(ratio.type, static_cast<u32>(ratio.ratio * static_cast<f32>(setCount)));
        }

        VkDescriptorPoolCreateInfo poolInfo =
        {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext         = nullptr,
            .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            .maxSets       = setCount,
            .poolSizeCount = static_cast<u32>(poolSizes.size()),
            .pPoolSizes    = poolSizes.data()
        };

        VkDescriptorPool pool = VK_NULL_HANDLE;
        Vk::CheckResult(vkCreateDescriptorPool(
            device,
            &poolInfo,
            nullptr,
            &pool),
            "Failed to create descriptor pool!"
        );

        return pool;
    }
}