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

#ifndef DESCRIPTOR_ALLOCATOR_H
#define DESCRIPTOR_ALLOCATOR_H

#include <span>
#include <vulkan/vulkan.h>

#include "DescriptorSet.h"
#include "Util/Util.h"

namespace Vk
{
    class DescriptorAllocator
    {
    public:
        struct PoolRatio
        {
            VkDescriptorType type;
            f32              ratio;
        };

        DescriptorAllocator() = default;
        DescriptorAllocator(VkDevice device, u32 initialSetCount, const std::span<const PoolRatio> ratios);

        Vk::DescriptorSet Allocate(VkDevice device, VkDescriptorSetLayout layout);

        void Clear(VkDevice device);
        void Destroy(VkDevice device);
    private:
        VkDescriptorPool GetPool(VkDevice device);
        VkDescriptorPool CreatePool(VkDevice device, u32 setCount, const std::span<const PoolRatio> ratios);

        std::vector<PoolRatio>        m_ratios      = {};
        std::vector<VkDescriptorPool> m_fullPools  = {};
        std::vector<VkDescriptorPool> m_readyPools = {};

        // Increase this for each new pool
        uint32_t m_setsPerPool = 0;
    };
}

#endif
