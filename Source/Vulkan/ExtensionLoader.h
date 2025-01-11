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

#ifndef EXTENSION_LOADER_H
#define EXTENSION_LOADER_H

#include <string_view>
#include <vulkan/vulkan.h>

#include "Util/Log.h"

/// There is absolutely no reason for this to be written in this way. Just absolutely none.
/// Please forgive my sins.

namespace Vk
{
    template <typename T>
    concept HasLoader = std::is_same_v<T, VkInstance> || std::is_same_v<T, VkDevice>;

    template<typename T>
    using LoaderType = PFN_vkVoidFunction (VKAPI_PTR*) (T, const char*);

    // PFN = Pointer to Function
    template<typename PFN, typename Param1Type, LoaderType<Param1Type> Loader>
        requires (std::is_pointer_v<PFN>) && (HasLoader<Param1Type>) && (Loader != nullptr)
    [[nodiscard]] PFN LoadExtension(Param1Type param1, std::string_view name)
    {
        auto extension = reinterpret_cast<PFN>(Loader(param1, name.data()));

        if (extension == nullptr)
        {
            Logger::Error("Failed to load function \"{}\" for {} \n", name, std::bit_cast<const void*>(param1));
        }

        Logger::Debug("Loaded function {} [address={}]\n", name, std::bit_cast<void*>(extension));

        return extension;
    }

    // Instance loader
    template <typename T>
    [[nodiscard]] T LoadExtension(VkInstance instance, std::string_view name)
    {
        return LoadExtension<T, VkInstance, vkGetInstanceProcAddr>(instance, name);
    }

    // Device loader
    template <typename T>
    [[nodiscard]] T LoadExtension(VkDevice device, std::string_view name)
    {
        return LoadExtension<T, VkDevice, vkGetDeviceProcAddr>(device, name);
    }
}

#endif
