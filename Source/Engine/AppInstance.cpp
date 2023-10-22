#include "AppInstance.h"

#include <SDL_vulkan.h>

#include "../Util/Log.h"
#include "../Vulkan/Extensions.h"

namespace Engine
{
    AppInstance::AppInstance()
        : m_window(std::make_unique<Window>())
    {
        // Initialise vulkan
        InitVulkan();
        // Log
        LOG_DEBUG("{}\n", "App instance initialised!");
    }

    void AppInstance::Run()
    {
        while (true)
        {
            // Poll events
            if (m_window->PollEvents()) break;
        }
    }

    void AppInstance::InitVulkan()
    {
        // Create vulkan instance
        CreateVKInstance();
        // Load extensions
        Vk::Extensions::Load(m_vkInstance);
        // Setup debugging
        SetupDebugCallback();
    }

    void AppInstance::CreateVKInstance()
    {
        // Application info
        VkApplicationInfo appInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext              = nullptr,
            .pApplicationName   = "Vulkan Renderer",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName        = "Rachit's Engine",
            .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion         = VK_API_VERSION_1_0
        };

        // Get extensions
        auto extensions = GetExtensions();

        // Request validation layers
        const std::vector<const char*> validationLayers = {"VK_LAYER_KHRONOS_validation"};
        LoadValidationLayers(validationLayers);

        // Instance creation info
        VkInstanceCreateInfo createInfo =
        {
            .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .pApplicationInfo        = &appInfo,
            .enabledLayerCount       = static_cast<uint32_t>(validationLayers.size()),
            .ppEnabledLayerNames     = validationLayers.data(),
            .enabledExtensionCount   = static_cast<uint32_t>(extensions.size()),
            .ppEnabledExtensionNames = extensions.data(),
        };

        // Create instance
        if (vkCreateInstance(&createInfo, nullptr, &m_vkInstance) != VK_SUCCESS)
        {
            // Vulkan initialisation failed, abort
            LOG_ERROR("{}\n", "Failed to initialise vulkan instance!");
        }

        // Log
        LOG_DEBUG("{}\n", "Successfully initialised Vulkan instance!");
    }

    std::vector<const char*> AppInstance::GetExtensions()
    {
        // Extensions count (from SDL)
        u32 extensionCount = 0;
        // Get extension count
        SDL_Vulkan_GetInstanceExtensions
        (
            m_window->handle,
            &extensionCount,
            nullptr
        );

        // Allocate memory for extensions
        auto instanceExtensions = reinterpret_cast<const char**>(SDL_malloc(sizeof(const char*) * extensionCount));
        // Get extensions for real this time
        auto extensionsLoaded = SDL_Vulkan_GetInstanceExtensions
        (
            m_window->handle,
            &extensionCount,
            instanceExtensions
        );

        // Make sure extensions were loaded
        if (extensionsLoaded == SDL_FALSE)
        {
            // Extension load failed, exit
            LOG_ERROR("Failed to load extensions!: {}\n", SDL_GetError());
        }

        // Convert to vector
        auto extensions = std::vector<const char*>(instanceExtensions, instanceExtensions + extensionCount);
        // Free extensions
        SDL_free(instanceExtensions);

        // Add other extensions
        extensions.emplace_back("VK_EXT_debug_utils");

        // Log
        LOG_DEBUG("Loaded extensions: {}\n", extensions.size());

        // Return
        return extensions;
    }

    void AppInstance::LoadValidationLayers(const std::vector<const char*>& layers)
    {
        // Check for availability
        if (!AreValidationLayersSupported(layers))
        {
            // Log
            LOG_ERROR("{}\n", "Validation layers not found!");
        }

        // Log
        LOG_DEBUG("Loaded validation layers: {}\n", layers.size());
    }

    bool AppInstance::AreValidationLayersSupported(const std::vector<const char*>& layers)
    {
        // Get layer count
        u32 layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        // Log
        LOG_DEBUG("Available validation layers: {}\n", layerCount);

        // Get layers
        auto availableLayers = std::vector<VkLayerProperties>(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

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

        // Found all the layers!
        return true;
    }

    void AppInstance::SetupDebugCallback()
    {
        VkDebugUtilsMessengerCreateInfoEXT createInfo =
        {
            .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .pNext           = nullptr,
            .flags           = 0,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT   |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
            .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT    |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = AppInstance::DebugCallback,
            .pUserData       = nullptr,
        };

        // Create a messenger
        auto result = vkCreateDebugUtilsMessengerEXT(m_vkInstance, &createInfo, nullptr, &m_messenger);
        // Check for errors
        if (result != VK_SUCCESS)
        {
            // Log
            LOG_ERROR("{}\n", "Failed to set up debug messenger!");
        }
    }

    VkBool32 VKAPI_CALL AppInstance::DebugCallback
    (
        UNUSED VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        UNUSED VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        UNUSED void* pUserData
    )
    {
        LOG_VK("{}\n", pCallbackData->pMessage);
        // Return
        return VK_TRUE;
    }

    AppInstance::~AppInstance()
    {
        // Destroy debugging utils
        vkDestroyDebugUtilsMessengerEXT(m_vkInstance, m_messenger, nullptr);
        // Delete extensions
        Vk::Extensions::Destroy();
        // Destroy vulkan instance
        vkDestroyInstance(m_vkInstance, nullptr);
        // Log
        LOG_DEBUG("{}\n", "App instance destroyed!");
    }
}