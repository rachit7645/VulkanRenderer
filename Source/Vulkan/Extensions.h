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

#ifndef EXTENSIONS_H
#define EXTENSIONS_H

#include <vector>
#include <string>
#include <vulkan/vulkan.h>

#include "Externals/UnorderedDense.h"

namespace Vk
{
    class Extensions
    {
    public:
        Extensions() = default;

        explicit Extensions(VkPhysicalDevice device);

        [[nodiscard]] static std::vector<const char*> GetInstanceExtensions();

        [[nodiscard]] std::vector<const char*> GetDeviceExtensions() const;

        [[nodiscard]] bool HasRequiredExtensions() const;
        [[nodiscard]] bool HasRayTracing()         const;
        [[nodiscard]] bool HasMemoryBudget()       const;
    private:
        [[nodiscard]] bool HasExtension(const std::string_view name) const;

        void QueryInstanceExtensions();
        void QueryDeviceExtensions(VkPhysicalDevice device);
        void QueryDeviceFeatures(VkPhysicalDevice device);

        std::vector<VkExtensionProperties> m_instanceExtensions;
        std::vector<VkExtensionProperties> m_deviceExtensions;

        ankerl::unordered_dense::map<std::string, bool> m_extensionTable;

        VkPhysicalDeviceAccelerationStructureFeaturesKHR  m_accelerationStructureFeatures = {};
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR     m_rayTracingPipelineFeatures    = {};
        VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR m_rayTracingMaintenanceFeatures = {};
    };
}

#endif
