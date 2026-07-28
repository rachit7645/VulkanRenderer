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

#include "Extensions.h"

#include "Util.h"
#include "Externals/SDL.h"
#include "Externals/UnorderedDense.h"
#include "Externals/DLSSNoSDK.h"
#include "Util/Log.h"
#include "Util/Types.h"

namespace Vk
{
    constexpr std::array REQUIRED_INSTANCE_EXTENSIONS =
    {
        VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
        VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME,
        #ifdef ENGINE_DEBUG
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
        #endif
    };

    constexpr std::array REQUIRED_DEVICE_EXTENSIONS =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_MAINTENANCE_1_EXTENSION_NAME,
        #ifdef ENGINE_DEBUG
        VK_KHR_SHADER_RELAXED_EXTENDED_INSTRUCTION_EXTENSION_NAME
        #endif
    };

    Extensions::Extensions(VkPhysicalDevice device)
    {
        QueryInstanceExtensions();
        QueryDeviceExtensions(device);
    }

    Stack::Vector<const char*> Extensions::GetInstanceExtensions(Stack::Allocator& allocator)
    {
        const auto SDLInstanceExtensions  = SDL::GetInstanceExtensions();
        const auto DLSSInstanceExtensions = DLSS::GetInstanceExtensions(allocator);

        usize totalExtensionCount = 0;

        totalExtensionCount += REQUIRED_INSTANCE_EXTENSIONS.size();
        totalExtensionCount += SDLInstanceExtensions.size();
        totalExtensionCount += DLSSInstanceExtensions.size();

        auto extensions = Stack::CreateVector<const char*>(allocator);

        extensions.reserve(totalExtensionCount);

        extensions.append_range(REQUIRED_INSTANCE_EXTENSIONS);
        extensions.append_range(SDLInstanceExtensions);
        extensions.append_range(DLSSInstanceExtensions);

        return extensions;
    }

    Stack::Vector<const char*> Extensions::GetDeviceExtensions(ENGINE_UNUSED VkInstance instance, ENGINE_UNUSED VkPhysicalDevice physicalDevice, Stack::Allocator& allocator) const
    {
        auto extensions = Stack::CreateVector<const char*>(allocator);

        extensions.append_range(REQUIRED_DEVICE_EXTENSIONS);

        if (HasMemoryBudget())
        {
            extensions.emplace_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
        }

        extensions.append_range(DLSS::GetDeviceExtensions(
            instance,
            physicalDevice,
            allocator
        ));

        return extensions;
    }

    bool Extensions::HasRequiredExtensions() const
    {
        const auto ExtensionChecker = [this] (const auto name) { return HasExtension(name); };

        const bool hasRequiredInstanceExtensions = std::ranges::all_of(REQUIRED_INSTANCE_EXTENSIONS, ExtensionChecker);
        const bool hasRequiredDeviceExtensions   = std::ranges::all_of(REQUIRED_DEVICE_EXTENSIONS,   ExtensionChecker);

        return hasRequiredInstanceExtensions && hasRequiredDeviceExtensions;
    }

    bool Extensions::HasMemoryBudget() const
    {
        return HasExtension(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
    }

    bool Extensions::HasExtension(const std::string_view name) const
    {
        const auto iter = m_extensionTable.find(name);

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
}
