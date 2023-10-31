#ifndef EXTENSION_LOADER_H
#define EXTENSION_LOADER_H

#include <string_view>
#include <vulkan/vulkan.h>

#include "Util/Log.h"

namespace Vk
{
    // Template to check type
    template <typename T>
    concept HasLoader = std::is_same_v<T, VkInstance> || std::is_same_v<T, VkDevice>;

    // Loader type
    template<typename T>
    using LoaderType = PFN_vkVoidFunction (VKAPI_PTR*) (T, const char*);

    // Template to load function
    template <typename PFN, typename Param1Type, LoaderType<Param1Type> Loader>
        requires (std::is_pointer_v<PFN>) && (HasLoader<Param1Type>) && (Loader != nullptr)
    PFN LoadExtension(Param1Type param1, std::string_view name)
    {
        // Load and return
        auto extension = reinterpret_cast<PFN>(Loader(param1, name.data()));
        // Make sure we were able to load extension
        if (extension == nullptr)
        {
            // Log
            LOG_ERROR("Failed to load function \"{}\" for {} \n", name, reinterpret_cast<const void*>(param1));
        }
        // Log
        LOG_DEBUG("Loaded function {} [address={}]\n", name, reinterpret_cast<void*>(extension));
        // Return
        return extension;
    }

    // Instance loader
    template <typename T>
    T LoadExtension(VkInstance instance, std::string_view name)
    {
        // Load
        return LoadExtension<T, VkInstance, vkGetInstanceProcAddr>(instance, name);
    }

    // Device loader
    template <typename T>
    T LoadExtension(VkDevice device, std::string_view name)
    {
        // Load
        return LoadExtension<T, VkDevice, vkGetDeviceProcAddr>(device, name);
    }
}

#endif
