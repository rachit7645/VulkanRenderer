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

#include "Extensions.h"

#include <volk/volk.h>

#include "Util.h"
#include "Util/Types.h"
#include "Util/Log.h"
#include "Externals/UnorderedDense.h"
#include "Externals/SDL.h"

namespace Vk
{
    constexpr std::array REQUIRED_INSTANCE_EXTENSIONS =
    {
        VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
        VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME,
        #ifdef ENGINE_DEBUG
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
        #endif
    };

    constexpr std::array REQUIRED_DEVICE_EXTENSIONS =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME,
        VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
        #ifdef ENGINE_DEBUG
        VK_KHR_SHADER_RELAXED_EXTENDED_INSTRUCTION_EXTENSION_NAME
        #endif
    };

    Extensions::Extensions(VkPhysicalDevice device)
    {
        QueryInstanceExtensions();
        QueryDeviceExtensions(device);
        QueryDeviceFeatures(device);
    }

    std::vector<const char*> Extensions::GetInstanceExtensions()
    {
        u32 extensionCount = 0;

        const auto instanceExtensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);

        if (instanceExtensions == nullptr)
        {
            Logger::Error("Failed to load extensions!: {}\n", SDL_GetError());
        }

        auto extensionStrings = std::vector(instanceExtensions, instanceExtensions + extensionCount);

        extensionStrings.insert(extensionStrings.end(), REQUIRED_INSTANCE_EXTENSIONS.begin(), REQUIRED_INSTANCE_EXTENSIONS.end());

        return extensionStrings;
    }

    std::vector<const char*> Extensions::GetDeviceExtensions() const
    {
        auto extensions = std::vector(REQUIRED_DEVICE_EXTENSIONS.begin(), REQUIRED_DEVICE_EXTENSIONS.end());

        if (HasRayTracing())
        {
            extensions.emplace_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
            extensions.emplace_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
            extensions.emplace_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
            extensions.emplace_back(VK_KHR_RAY_TRACING_MAINTENANCE_1_EXTENSION_NAME);
        }

        return extensions;
    }

    bool Extensions::HasRequiredExtensions() const
    {
        const auto ExtensionChecker = [this] (const auto name) { return HasExtension(name); };

        const bool hasRequiredInstanceExtensions = std::ranges::all_of(REQUIRED_INSTANCE_EXTENSIONS, ExtensionChecker);
        const bool hasRequiredDeviceExtensions   = std::ranges::all_of(REQUIRED_DEVICE_EXTENSIONS,   ExtensionChecker);

        return hasRequiredInstanceExtensions && hasRequiredDeviceExtensions;
    }

    bool Extensions::HasRayTracing() const
    {
        const bool hasASExtension = HasExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        const bool hasASFeature   = m_accelerationStructureFeatures.accelerationStructure;
        const bool hasAS          = hasASExtension && hasASFeature;

        const bool hasDeferredHostOperations = HasExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);

        const bool hasRayTracingPipelineExtension = HasExtension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
        const bool hasRayTracingPipelineFeature   = m_rayTracingPipelineFeatures.rayTracingPipeline;
        const bool hasRayTracingPipeline          = hasRayTracingPipelineExtension && hasRayTracingPipelineFeature;

        const bool hasRayTracingMaintenanceExtension = HasExtension(VK_KHR_RAY_TRACING_MAINTENANCE_1_EXTENSION_NAME);
        const bool hasRayTracingMaintenanceFeature   = m_rayTracingMaintenanceFeatures.rayTracingMaintenance1;
        const bool hasRayTracingMaintenance          = hasRayTracingMaintenanceExtension && hasRayTracingMaintenanceFeature;

        return hasAS && hasDeferredHostOperations && hasRayTracingPipeline && hasRayTracingMaintenance;
    }

    bool Extensions::HasExtension(const std::string_view name) const
    {
        const auto iter = m_extensionTable.find(name.data());

        if (iter == m_extensionTable.cend())
        {
            return false;
        }

        return iter->second;
    }

    void Extensions::QueryInstanceExtensions()
    {
        u32 extensionCount = 0;

        Vk::CheckResult(vkEnumerateInstanceExtensionProperties(
            nullptr,
            &extensionCount,
            nullptr),
            "Failed to query instance extension count!"
        );

        if (extensionCount == 0)
        {
            Logger::Warning("{}\n", "Failed to find any instance extensions!");
        }

        m_instanceExtensions = std::vector<VkExtensionProperties>(extensionCount);

        Vk::CheckResult(vkEnumerateInstanceExtensionProperties(
            nullptr,
            &extensionCount,
            m_instanceExtensions.data()),
            "Failed to enumerate instance extensions!"
        );

        for (const auto& [name, version] : m_instanceExtensions)
        {
            m_extensionTable[name] = true;
        }
    }

    void Extensions::QueryDeviceExtensions(VkPhysicalDevice device)
    {
        u32 extensionCount = 0;

        Vk::CheckResult(vkEnumerateDeviceExtensionProperties(
            device,
            nullptr,
            &extensionCount,
            nullptr),
            "Failed to query device extension count!"
        );

        if (extensionCount == 0)
        {
            Logger::Warning("Failed to find any extensions! [Physical Device={}]\n", std::bit_cast<void*>(device));
        }

        m_deviceExtensions = std::vector<VkExtensionProperties>(extensionCount);

        Vk::CheckResult(vkEnumerateDeviceExtensionProperties(
            device,
            nullptr,
            &extensionCount,
            m_deviceExtensions.data()),
            "Failed to enumerate device extensions!"
        );

        for (const auto& [name, version] : m_deviceExtensions)
        {
            m_extensionTable[name] = true;
        }
    }

    void Extensions::QueryDeviceFeatures(VkPhysicalDevice device)
    {
        m_rayTracingMaintenanceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_MAINTENANCE_1_FEATURES_KHR;
        m_rayTracingMaintenanceFeatures.pNext = nullptr;

        m_rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
        m_rayTracingPipelineFeatures.pNext = &m_rayTracingMaintenanceFeatures;

        m_accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        m_accelerationStructureFeatures.pNext = &m_rayTracingPipelineFeatures;

        VkPhysicalDeviceFeatures2 features = {};
        features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features.pNext = &m_accelerationStructureFeatures;

        vkGetPhysicalDeviceFeatures2(device, &features);
    }
}