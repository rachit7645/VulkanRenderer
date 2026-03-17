/*
 * Copyright (c) 2023 - 2026 Rachit
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

#include "Util/Concept.h"
#include "Util/Log.h"

namespace Vk
{
    template<typename T>
    struct StructureType;

    template<> struct StructureType<VkPhysicalDeviceVulkan13Features>
    {
        static constexpr VkStructureType sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    };

    template<> struct StructureType<VkPhysicalDeviceVulkan12Features>
    {
        static constexpr VkStructureType sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    };

    template<> struct StructureType<VkPhysicalDeviceVulkan11Features>
    {
        static constexpr VkStructureType sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    };

    template<> struct StructureType<VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT>
    {
        static constexpr VkStructureType sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT;
    };

    template<> struct StructureType<VkPhysicalDeviceAccelerationStructureFeaturesKHR>
    {
        static constexpr VkStructureType sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    };

    template<> struct StructureType<VkPhysicalDeviceRayTracingPipelineFeaturesKHR>
    {
        static constexpr VkStructureType sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    };

    template<> struct StructureType<VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR>
    {
        static constexpr VkStructureType sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_MAINTENANCE_1_FEATURES_KHR;
    };

    template<> struct StructureType<VkPhysicalDeviceShaderRelaxedExtendedInstructionFeaturesKHR>
    {
        static constexpr VkStructureType sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_RELAXED_EXTENDED_INSTRUCTION_FEATURES_KHR;
    };

    template<> struct StructureType<VkPhysicalDeviceVulkan11Properties>
    {
        static constexpr VkStructureType sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
    };

    template<typename T>
    std::optional<T*> FindStructureInChainOptional(void* pNext)
    {
        for (auto current = static_cast<VkBaseOutStructure*>(pNext); current != nullptr; current = current->pNext)
        {
            if (current->sType == Vk::StructureType<T>::sType)
            {
                return reinterpret_cast<T*>(current);
            }
        }

        return std::nullopt;
    }

    template<typename T>
    std::optional<const T*> FindStructureInChainOptional(const void* pNext)
    {
        for (auto current = static_cast<const VkBaseInStructure*>(pNext); current != nullptr; current = current->pNext)
        {
            if (current->sType == Vk::StructureType<T>::sType)
            {
                return reinterpret_cast<const T*>(current);
            }
        }

        return std::nullopt;
    }

    template<typename T>
    T* FindStructureInChain(void* pNext)
    {
        auto result = Vk::FindStructureInChainOptional<T>(pNext);

        if (!result.has_value())
        {
            Logger::Error("Failed to find structure in chain! [pNext={}]", pNext);
        }

        return result.value();
    }

    template<typename T>
    const T* FindStructureInChain(const void* pNext)
    {
        auto result = Vk::FindStructureInChainOptional<T>(pNext);

        if (!result.has_value())
        {
            Logger::Error("Failed to find structure in chain! [pNext={}]", pNext);
        }

        return result.value();
    }
}

#endif
