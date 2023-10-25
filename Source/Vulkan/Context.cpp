#include "Context.h"

#include <map>
#include <unordered_map>
#include <set>

#include "Extensions.h"
#include "../Util/Log.h"

namespace Vk
{
    #ifdef ENGINE_DEBUG
    // Layers
    const std::vector<const char*> VALIDATION_LAYERS = {"VK_LAYER_KHRONOS_validation"};
    #endif

    Context::Context(SDL_Window* window)
        : m_extensions(std::make_unique<Vk::Extensions>())
    {
        // Create vulkan instance
        CreateVKInstance(window);
        // Load extensions
        m_extensions->LoadFunctions(vkInstance);
        #ifdef ENGINE_DEBUG
        // Load validation layers
        if (m_layers->SetupMessenger(vkInstance) != VK_SUCCESS)
        {
            // Log
            LOG_ERROR("{}\n", "Failed to set up debug messenger!");
        }
        #endif
        // Create surface
        CreateSurface(window);
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
        auto extensions = m_extensions->LoadInstanceExtensions(window);
        // Create extensions string
        std::string extDbg = fmt::format("Loaded vulkan instance extensions: {}\n", extensions.size());
        for (auto&& extension : extensions)
        {
            // Add
            extDbg.append(fmt::format("- {}\n", extension));
        }
        // Log
        LOG_DEBUG("{}", extDbg);

        #ifdef ENGINE_DEBUG
        // Request validation layers
        m_layers = std::make_unique<Vk::ValidationLayers>(VALIDATION_LAYERS);
        #endif

        // Instance creation info
        VkInstanceCreateInfo createInfo =
        {
            .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            #ifdef ENGINE_DEBUG
            .pNext                   = (VkDebugUtilsMessengerCreateInfoEXT*) &m_layers->messengerInfo,
            #else
            .pNext                   = nullptr,
            #endif
            .flags                   = 0,
            .pApplicationInfo        = &appInfo,
            #ifdef ENGINE_DEBUG
            .enabledLayerCount       = static_cast<u32>(VALIDATION_LAYERS.size()),
            .ppEnabledLayerNames     = VALIDATION_LAYERS.data(),
            #else
            .enabledLayerCount       = 0,
            .ppEnabledLayerNames     = nullptr,
            #endif
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
        LOG_INFO("Successfully initialised Vulkan instance! [handle={}]\n", reinterpret_cast<void*>(vkInstance));
    }

    void Context::CreateSurface(SDL_Window* window)
    {
        // Create surface using SDL
        if (SDL_Vulkan_CreateSurface(window, vkInstance, &m_surface) != SDL_TRUE)
        {
            // Log
            LOG_ERROR
            (
                "Failed to create surface! [window={}] [instance={}]\n",
                reinterpret_cast<void*>(window),
                reinterpret_cast<void*>(vkInstance)
            );
        }

        // Log
        LOG_INFO("Initialised vulkan surface! [handle={}]\n", reinterpret_cast<void*>(m_surface));
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
        std::string dbgPhyDevices = fmt::format("Available physical devices: {}\n", deviceCount);
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

            // Get queue
            auto queue = FindQueueFamilies(device);

            // Calculate score parts
            usize discreteGPU = (propertySet.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) * 10000;
            // Calculate score multipliers
            bool isQueueValid = queue.IsComplete() && queue.IsPerformant();

            // Calculate score
            usize score = isQueueValid * discreteGPU;
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

    Vk::QueueFamilyIndices Context::FindQueueFamilies(VkPhysicalDevice device)
    {
        // Indices
        Vk::QueueFamilyIndices indices = {};

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
            // Check for presentation support
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR
            (
                device,
                i,
                m_surface,
                &presentSupport
            );

            if (presentSupport == VK_TRUE)
            {
                // Found present family! (hopefully it's the same as the graphics family)
                indices.presentFamily = i;
            }

            // Check if family has graphics support
            if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                // Found graphics family!
                indices.graphicsFamily = i;
            }

            // Check for completeness
            if (indices.IsComplete() && indices.IsPerformant())
            {
                // Found all the queue families we need
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

        // Queue priority
        constexpr f32 queuePriority = 1.0f;

        // Queues data
        std::set<u32> uniqueQueueFamilies = {m_queueFamilies.graphicsFamily.value(), m_queueFamilies.presentFamily.value()};
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

        // Create queue creation data
        for (auto&& queueFamily : uniqueQueueFamilies)
        {
            // Queue creation info
            VkDeviceQueueCreateInfo queueCreateInfo =
            {
                .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext            = nullptr,
                .flags            = 0,
                .queueFamilyIndex = queueFamily,
                .queueCount       = 1,
                .pQueuePriorities = &queuePriority
            };
            // Add to vector
            queueCreateInfos.push_back(queueCreateInfo);
        }

        // Vulkan device features
        VkPhysicalDeviceFeatures deviceFeatures = {};

        // Logical device creation info
        VkDeviceCreateInfo createInfo =
        {
            .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .queueCreateInfoCount    = static_cast<u32>(queueCreateInfos.size()),
            .pQueueCreateInfos       = queueCreateInfos.data(),
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

        // Get queue
        vkGetDeviceQueue
        (
            m_logicalDevice,
            m_queueFamilies.presentFamily.value(),
            0,
            &m_graphicsQueue
        );
    }

    Context::~Context()
    {
        // Destroy surface
        vkDestroySurfaceKHR(vkInstance, m_surface, nullptr);
        #ifdef ENGINE_DEBUG
        // Destroy validation layers
        m_layers->DestroyMessenger(vkInstance);
        #endif
        // Destroy logical device
        vkDestroyDevice(m_logicalDevice, nullptr);
        // Destroy vulkan instance
        vkDestroyInstance(vkInstance, nullptr);
        // Log
        LOG_INFO("{}\n", "Destroyed vulkan context!");
    }
}