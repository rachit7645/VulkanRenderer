#include "ValidationLayers.h"

#include "../Util/Log.h"

namespace Vk
{
#ifdef ENGINE_DEBUG
    ValidationLayers::ValidationLayers(const std::span<const char* const> layers)
    {
        // Check for availability
        if (!CheckLayers(layers))
        {
            // Log
            LOG_ERROR("{}\n", "Validation layers not found!");
        }

        // Creation data
        messengerInfo =
        {
            .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .pNext           = nullptr,
            .flags           = 0,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT   |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
            .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT    |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = ValidationLayers::DebugCallback,
            .pUserData       = nullptr,
        };
    }

    VkResult ValidationLayers::SetupMessenger(VkInstance instance)
    {
        // Create a messenger
        return vkCreateDebugUtilsMessengerEXT
        (
            instance,
            &messengerInfo,
            nullptr,
            &messenger
        );
    }

    bool ValidationLayers::CheckLayers(const std::span<const char* const> layers)
    {
        // Get layer count
        u32 layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        // Get layers
        auto availableLayers = std::vector<VkLayerProperties>(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        // Create layers string
        std::string layerDbg = fmt::format("Available vulkan validation layers: {}\n", layerCount);
        // Create unique layers set
        auto requiredLayers = std::set<std::string_view>(layers.begin(), layers.end());

        // Go over all layer properties
        for (auto&& layerProperties : availableLayers)
        {
            // Add to string
            layerDbg.append(fmt::format("- {}\n", layerProperties.layerName));
            // Remove from set
            requiredLayers.erase(layerProperties.layerName);
        }

        // Log
        LOG_DEBUG("{}", layerDbg);

        // Found all the layers!
        return requiredLayers.empty();
    }

    void ValidationLayers::DestroyMessenger(VkInstance instance)
    {
        // Destroy
        vkDestroyDebugUtilsMessengerEXT(instance, messenger, nullptr);
        messenger = nullptr;
    }

    // TODO: Improve validation layer debug callback
    VKAPI_ATTR VkBool32 VKAPI_CALL ValidationLayers::DebugCallback
    (
        UNUSED VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        UNUSED VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        UNUSED void* pUserData
    )
    {
        // Only log above a certain severity
        if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        {
            // Log
            LOG_VK("{}\n", pCallbackData->pMessage);
        }
        // Return
        return VK_TRUE;
    }
#endif
}