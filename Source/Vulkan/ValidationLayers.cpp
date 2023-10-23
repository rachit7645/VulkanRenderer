#include "ValidationLayers.h"

#include "../Util/Log.h"
#include "../Util/Util.h"

namespace Vk
{
    ValidationLayers::ValidationLayers(const std::vector<const char*>& layers)
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

    bool ValidationLayers::CheckLayers(const std::vector<const char*>& layers)
    {
        // Get layer count
        u32 layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        // Get layers
        auto availableLayers = std::vector<VkLayerProperties>(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        // Create layers string
        std::string layerDbg = fmt::format("Available vulkan validation layers: {}\n", layerCount);
        // Go over all layer properties
        for (auto&& layerProperties : availableLayers)
        {
            // Add to string
            layerDbg.append(fmt::format("- {}\n", layerProperties.layerName));
        }

        // Check all required layers
        for (auto&& layer : layers)
        {
            // Flag
            auto layerFound = false;

            // Try to find required layer
            for (auto&& layerProperties : availableLayers)
            {
                if (strcmp(layer, layerProperties.layerName) == 0)
                {
                    layerFound = true;
                    break;
                }
            }

            // If any layer was not found
            if (!layerFound)
            {
                return false;
            }
        }

        // Log
        LOG_DEBUG("{}", layerDbg);

        // Found all the layers!
        return true;
    }

    void ValidationLayers::DestroyMessenger(VkInstance instance)
    {
        // Destroy
        vkDestroyDebugUtilsMessengerEXT(instance, messenger, nullptr);
        messenger = nullptr;
    }

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
}