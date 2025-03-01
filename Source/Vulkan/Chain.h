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

#ifndef STRUCTURE_CHAIN_H
#define STRUCTURE_CHAIN_H

#include <vulkan/vulkan.h>

#include "Util/Log.h"

namespace Vk
{
    template<typename T>
    struct VulkanStructType;

    template<> struct VulkanStructType<VkPhysicalDeviceVulkan13Features>
    {
        static constexpr VkStructureType sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    };

    template<> struct VulkanStructType<VkPhysicalDeviceVulkan12Features>
    {
        static constexpr VkStructureType sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    };

    template<> struct VulkanStructType<VkPhysicalDeviceVulkan11Features>
    {
        static constexpr VkStructureType sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    };

    template<> struct VulkanStructType<VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT>
    {
        static constexpr VkStructureType sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT;
    };

    template<typename T>
    T* FindStructureInChain(void* pNext)
    {
        for (auto current = static_cast<VkBaseOutStructure*>(pNext); current != nullptr; current = current->pNext)
        {
            if (current->sType == VulkanStructType<T>::sType)
            {
                return reinterpret_cast<T*>(current);
            }
        }

        Logger::VulkanError("Failed to find structure in chain! [pNext={}]", pNext);
    }

    template<typename T>
    const T* FindStructureInChain(const void* pNext)
    {
        for (auto current = static_cast<const VkBaseOutStructure*>(pNext); current != nullptr; current = current->pNext)
        {
            if (current->sType == VulkanStructType<T>::sType)
            {
                return reinterpret_cast<const T*>(current);
            }
        }

        Logger::VulkanError("Failed to find structure in chain! [pNext={}]", pNext);
    }
}

#endif
