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

#ifndef DESCRIPTOR_CACHE_H
#define DESCRIPTOR_CACHE_H

#include <unordered_map>

#include "DescriptorSet.h"
#include "DescriptorAllocator.h"
#include "Constants.h"

namespace Vk
{
    class DescriptorCache
    {
    public:
        DescriptorCache()  = default;
        ~DescriptorCache() = default;

        explicit DescriptorCache(VkDevice device);

        // No copying
        DescriptorCache(const DescriptorCache&)            = delete;
        DescriptorCache& operator=(const DescriptorCache&) = delete;

        // Only moving
        DescriptorCache(DescriptorCache&& other)            noexcept = default;
        DescriptorCache& operator=(DescriptorCache&& other) noexcept = default;

        VkDescriptorSetLayout AddLayout(const std::string& ID, VkDevice device, VkDescriptorSetLayout layout);
        VkDescriptorSetLayout GetLayout(const std::string& ID);

        const Vk::DescriptorSet& AllocateSet
        (
            const std::string& ID,
            const std::string& layoutID,
            VkDevice device
        );

        const std::array<Vk::DescriptorSet, Vk::FRAMES_IN_FLIGHT>& AllocateSets
        (
            const std::string& ID,
            const std::string& layoutID,
            VkDevice device
        );

        const Vk::DescriptorSet& GetSet(const std::string& ID);
        const std::array<Vk::DescriptorSet, Vk::FRAMES_IN_FLIGHT>& GetSets(const std::string& ID);

        void Destroy(VkDevice device);
    private:
        DescriptorAllocator m_allocator = {};

        std::unordered_map<std::string, VkDescriptorSetLayout> m_layouts = {};

        std::unordered_map<std::string, Vk::DescriptorSet>                                   m_descriptorMap        = {};
        std::unordered_map<std::string, std::array<Vk::DescriptorSet, Vk::FRAMES_IN_FLIGHT>> m_descriptorsPerFIFMap = {};
    };
}

#endif
