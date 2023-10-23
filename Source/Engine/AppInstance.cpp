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
        Vk::Extensions::GetInstance().LoadFunctions(m_vkInstance);
        // Load validation layers
        if (m_layers->SetupMessenger(m_vkInstance) != VK_SUCCESS)
        {
            // Log
            LOG_ERROR("{}\n", "Failed to set up debug messenger!");
        }
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
        auto extensions = Vk::Extensions::GetInstance().LoadExtensions(m_window->handle);
        // Create extensions string
        std::string extDbg = fmt::format("Loaded vulkan instance extensions: {}\n", extensions.size());
        for (auto&& extension : extensions)
        {
            // Add
            extDbg.append(fmt::format("- {}\n", extension));
        }
        // Log
        LOG_DEBUG("{}", extDbg);

        // Request validation layers
        const std::vector<const char*> validationLayers = {"VK_LAYER_KHRONOS_validation"};
        m_layers = std::make_unique<Vk::ValidationLayers>(validationLayers);

        // Instance creation info
        VkInstanceCreateInfo createInfo =
        {
            .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext                   = (VkDebugUtilsMessengerCreateInfoEXT*) &m_layers->messengerInfo,
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

    AppInstance::~AppInstance()
    {
        // Destroy validation layers
        m_layers->DestroyMessenger(m_vkInstance);
        // Destroy objects
        Vk::Extensions::GetInstance().Destroy();
        // Destroy vulkan instance
        vkDestroyInstance(m_vkInstance, nullptr);
        // Log
        LOG_DEBUG("{}\n", "App instance destroyed!");
    }
}