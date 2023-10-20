#include "AppInstance.h"

#include <SDL_vulkan.h>

#include "../Util/Log.h"

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

        // Extensions count (from SDL)
        uint32_t extensionCount = 0;
        // Get extension count
        SDL_Vulkan_GetInstanceExtensions
        (
            m_window->handle,
            &extensionCount,
            nullptr
        );

        // Allocate memory for extensions
        auto extensions = reinterpret_cast<const char**>(SDL_malloc(sizeof(const char *) * extensionCount));
        // Get extensions for real this time
        auto extensionsLoaded = SDL_Vulkan_GetInstanceExtensions
        (
            m_window->handle,
            &extensionCount,
            extensions
        );

        // Make sure extensions were loaded
        if (extensionsLoaded == SDL_FALSE)
        {
            // Extension load failed, exit
            LOG_ERROR("Failed to load extensions!: {}\n", SDL_GetError());
        }

        // Instance creation info
        VkInstanceCreateInfo createInfo =
        {
            .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .pApplicationInfo        = &appInfo,
            .enabledLayerCount       = 0,
            .ppEnabledLayerNames     = nullptr,
            .enabledExtensionCount   = extensionCount,
            .ppEnabledExtensionNames = extensions,
        };

        // Create instance
        if (vkCreateInstance(&createInfo, nullptr, &vkInstance) != VK_SUCCESS)
        {
            // Vulkan initialisation failed, abort
            LOG_ERROR("{}\n", "Failed to initialise vulkan instance!");
        }

        // Log
        LOG_DEBUG("{}\n", "Successfully initialised Vulkan instance!");
    }

    AppInstance::~AppInstance()
    {
        // Destroy vulkan instance
        vkDestroyInstance(vkInstance, nullptr);
        // Log
        LOG_DEBUG("{}\n", "App instance destroyed!");
    }
}