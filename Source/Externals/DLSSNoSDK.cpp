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

#include "DLSSNoSDK.h"

#ifdef ENGINE_DLSS
#include "DLSS.h"
#endif

namespace DLSS
{
    Scratch::Vector<const char*> GetInstanceExtensions(ENGINE_UNUSED Scratch::Allocator& allocator)
    {
    #ifdef ENGINE_DLSS
        u32                    DLSSExtensionCount      = 0;
        VkExtensionProperties* DLSSExtensionProperties = nullptr;

        const NVSDK_NGX_Result result = NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements
        (
            &DLSS::FEATURE_DISCOVERY_INFO,
            &DLSSExtensionCount,
            &DLSSExtensionProperties
        );

        if (result != NVSDK_NGX_Result_Success)
        {
            return {};
        }

        auto extensions = Scratch::CreateVector<const char*>(allocator);

        extensions.resize(DLSSExtensionCount);

        for (u32 i = 0; i < DLSSExtensionCount; ++i)
        {
            extensions[i] = DLSSExtensionProperties[i].extensionName;
        }

        return extensions;
    #else
        return {};
    #endif
    }

    Scratch::Vector<const char*> GetDeviceExtensions
    (
        ENGINE_UNUSED VkInstance instance,
        ENGINE_UNUSED VkPhysicalDevice physicalDevice,
        ENGINE_UNUSED Scratch::Allocator& allocator
    )
    {
        #ifdef ENGINE_DLSS
        u32                    DLSSExtensionCount      = 0;
        VkExtensionProperties* DLSSExtensionProperties = nullptr;

        const NVSDK_NGX_Result result = NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements
        (
            instance,
            physicalDevice,
            &DLSS::FEATURE_DISCOVERY_INFO,
            &DLSSExtensionCount,
            &DLSSExtensionProperties
        );

        if (result != NVSDK_NGX_Result_Success)
        {
            return {};
        }

        auto extensions = Scratch::CreateVector<const char*>(allocator);

        extensions.resize(DLSSExtensionCount);

        for (u32 i = 0; i < DLSSExtensionCount; ++i)
        {
            extensions[i] = DLSSExtensionProperties[i].extensionName;
        }

        return extensions;
        #else
        return {};
        #endif
    }
}