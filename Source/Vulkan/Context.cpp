#include "Context.h"

#include <map>
#include <unordered_map>

#include "Extensions.h"
#include "../Util/Log.h"

namespace Vk
{
    // Layers
    const std::vector<const char*> VALIDATION_LAYERS = {"VK_LAYER_KHRONOS_validation"};

    Context::Context(SDL_Window* window)
    {
        // Create vulkan instance
        CreateVKInstance(window);
        // Load extensions
        Vk::Extensions::GetInstance().LoadFunctions(vkInstance);
        // Load validation layers
        if (m_layers->SetupMessenger(vkInstance) != VK_SUCCESS)
        {
            // Log
            LOG_ERROR("{}\n", "Failed to set up debug messenger!");
        }
        // Grab a GPU
        PickPhysicalDevice();
        // Setup logical device
        CreateLogicalDevice();
        // Log
        LOG_INFO("{}\n", "Initialised vulkan context!");
    }

    void Context::CreateVKInstance(SDL_Window* window)
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
        auto extensions = Vk::Extensions::GetInstance().LoadExtensions(window);
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
        m_layers = std::make_unique<Vk::ValidationLayers>(VALIDATION_LAYERS);

        // Instance creation info
        VkInstanceCreateInfo createInfo =
        {
            .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext                   = (VkDebugUtilsMessengerCreateInfoEXT*) &m_layers->messengerInfo,
            .flags                   = 0,
            .pApplicationInfo        = &appInfo,
            .enabledLayerCount       = static_cast<u32>(VALIDATION_LAYERS.size()),
            .ppEnabledLayerNames     = VALIDATION_LAYERS.data(),
            .enabledExtensionCount   = static_cast<u32>(extensions.size()),
            .ppEnabledExtensionNames = extensions.data(),
        };

        // Create instance
        if (vkCreateInstance(&createInfo, nullptr, &vkInstance) != VK_SUCCESS)
        {
            // Vulkan initialisation failed, abort
            LOG_ERROR("{}\n", "Failed to initialise vulkan instance!");
        }

        // Log
        LOG_INFO("Successfully initialised Vulkan instance! [instance={}]\n", reinterpret_cast<void*>(vkInstance));
    }

    void Context::PickPhysicalDevice()
    {
        // Get device count
        u32 deviceCount = 0;
        vkEnumeratePhysicalDevices(vkInstance, &deviceCount, nullptr);

        // Make sure we have at least one GPU that supports Vulkan
        if (deviceCount == 0)
        {
            // Log
            LOG_ERROR("No physical devices found! [instance = {}]\n", reinterpret_cast<void*>(vkInstance));
        }

        // Physical devices
        auto devices = std::vector<VkPhysicalDevice>(deviceCount);
        vkEnumeratePhysicalDevices(vkInstance, &deviceCount, devices.data());

        // Debug list
        std::string dbgPhyDevices = std::format("Available physical devices: {}\n", deviceCount);
        // Devices data
        auto properties = std::unordered_map<VkPhysicalDevice, VkPhysicalDeviceProperties>(deviceCount);
        auto features   = std::unordered_map<VkPhysicalDevice, VkPhysicalDeviceFeatures>(deviceCount);
        // Device scores
        auto scores = std::map<usize, VkPhysicalDevice>{};


        // Get information about physical devices
        for (auto&& device : devices)
        {
            // Get data
            VkPhysicalDeviceProperties propertySet;
            vkGetPhysicalDeviceProperties(device, &propertySet);
            VkPhysicalDeviceFeatures featureSet;
            vkGetPhysicalDeviceFeatures(device, &featureSet);

            // Load data
            properties.emplace(device, propertySet);
            features.emplace(device, featureSet);

            // Add to string
            dbgPhyDevices.append(fmt::format
            (
                "- {} [Driver Version: {}] [API Version: {}] [ID = {}] \n",
                propertySet.deviceName,
                propertySet.driverVersion,
                propertySet.apiVersion,
                propertySet.deviceID
            ));

            // Calculate score parts
            usize discreteGPU = (propertySet.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) * 10000;
            usize API         = propertySet.apiVersion;
            usize queue       = FindQueueFamilies(device).graphicsFamily.has_value();
            // Calculate score
            usize score = queue * (discreteGPU + API);
            scores.emplace(score, device);
        }

        // Log string
        LOG_DEBUG("{}", dbgPhyDevices);

        // Best GPU => Highest score
        auto [highestScore, bestDevice] = *scores.rbegin();
        // Make sure device is valid
        if (highestScore == 0)
        {
            // Log
            LOG_ERROR("Failed to load physical device! [name=\"{}\"]", properties[bestDevice].deviceName);
        }
        // Set GPU
        m_physicalDevice = bestDevice;

        // Log
        LOG_INFO("Selecting GPU: {}\n", properties[m_physicalDevice].deviceName);
    }

    Vk::QueueFamilyIndex Context::FindQueueFamilies(VkPhysicalDevice device)
    {
        // Indices
        Vk::QueueFamilyIndex indices = {};

        // Get queue family count
        u32 queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties
        (
        device,
        &queueFamilyCount,
        nullptr
        );

        // Get queue families
        auto queueFamilies = std::vector<VkQueueFamilyProperties>(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties
        (
        device,
        &queueFamilyCount,
        queueFamilies.data()
        );

        // Loop over queue families
        for (usize i = 0; auto&& family : queueFamilies)
        {
            // Find a family with graphics support
            if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                // Set index
                indices.graphicsFamily = i;
                break;
            }
            // Go to next family index
            ++i;
        }

        // Return
        return indices;
    }

    void Context::CreateLogicalDevice()
    {
        // Get queue
        m_queueFamilies = FindQueueFamilies(m_physicalDevice);

        // Queue data
        constexpr f32 queuePriority = 1.0f;

        // Queue creation info
        VkDeviceQueueCreateInfo queueCreateInfo =
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = m_queueFamilies.graphicsFamily.value(),
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        };

        // Vulkan device features
        VkPhysicalDeviceFeatures deviceFeatures = {};

        // Logical device creation info
        VkDeviceCreateInfo createInfo =
        {
            .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .queueCreateInfoCount    = 1,
            .pQueueCreateInfos       = &queueCreateInfo,
            .enabledLayerCount       = static_cast<u32>(VALIDATION_LAYERS.size()),
            .ppEnabledLayerNames     = VALIDATION_LAYERS.data(),
            .enabledExtensionCount   = 0,
            .ppEnabledExtensionNames = nullptr,
            .pEnabledFeatures        = &deviceFeatures
        };

        // Create device
        if (vkCreateDevice(
            m_physicalDevice,
            &createInfo,
            nullptr,
            &m_logicalDevice
        ) != VK_SUCCESS)
        {
            // Log
            LOG_ERROR("Failed to create logical device! [Physical Device = {}]\n", reinterpret_cast<void*>(m_physicalDevice));
        }

        // Log
        LOG_INFO("Successfully created vulkan logical device! [address={}]\n", reinterpret_cast<void*>(m_logicalDevice));
    }

    Context::~Context()
    {
        // Destroy validation layers
        m_layers->DestroyMessenger(vkInstance);
        // Destroy logical device
        vkDestroyDevice(m_logicalDevice, nullptr);
        // Destroy extensions
        Vk::Extensions::GetInstance().Destroy();
        // Destroy vulkan instance
        vkDestroyInstance(vkInstance, nullptr);
        // Log
        LOG_INFO("{}\n", "Destroyed vulkan context!");
    }
}