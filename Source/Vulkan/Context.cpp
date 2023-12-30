/*
 *    Copyright 2023 Rachit Khandelwal
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "Context.h"

#include <map>
#include <unordered_map>
#include <vulkan/vk_enum_string_helper.h>

#include "Extensions.h"
#include "SwapchainInfo.h"
#include "Util/Log.h"

namespace Vk
{
    // Constants
    constexpr auto VULKAN_API_VERSION = VK_API_VERSION_1_3;

    // Layers
    #ifdef ENGINE_DEBUG
    constexpr std::array<const char*, 1> VALIDATION_LAYERS = {"VK_LAYER_KHRONOS_validation"};
    #endif

    // Required device extensions
    #ifdef ENGINE_DEBUG
    constexpr std::array<const char*, 1> REQUIRED_EXTENSIONS = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    #else
    constexpr std::array<const char*, 1> REQUIRED_EXTENSIONS = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    #endif

    Context::Context(const std::shared_ptr<Engine::Window>& window)
    {
        // Create vulkan instance
        CreateInstance(window->handle);

        #ifdef ENGINE_DEBUG
        // Load validation layers
        if (m_layers.SetupMessenger(instance) != VK_SUCCESS)
        {
            // Log
            Logger::Error("{}\n", "Failed to set up debug messenger!");
        }
        #endif

        // Create surface
        CreateSurface(window->handle);
        // Grab a GPU
        PickPhysicalDevice();
        // Setup logical device
        CreateLogicalDevice();

        // Create global command pool
        CreateCommandPool();
        // Create descriptor pool
        CreateDescriptorPool();
        // Create allocator
        CreateAllocator();

        // Log
        Logger::Info("{}\n", "Initialised vulkan context!");
    }

    std::vector<VkDescriptorSet> Context::AllocateDescriptorSets(const std::span<VkDescriptorSetLayout> descriptorLayouts)
    {
        // Allocation info
        VkDescriptorSetAllocateInfo allocInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext              = nullptr,
            .descriptorPool     = descriptorPool,
            .descriptorSetCount = static_cast<u32>(descriptorLayouts.size()),
            .pSetLayouts        = descriptorLayouts.data()
        };

        // Descriptor sets
        auto descriptorSets = std::vector<VkDescriptorSet>(descriptorLayouts.size(), VK_NULL_HANDLE);

        // Allocate
        VkResult result = vkAllocateDescriptorSets
        (
            device,
            &allocInfo,
            descriptorSets.data()
        );

        // Check result
        if (result != VK_SUCCESS)
        {
            // Log
            Logger::VulkanError
            (
                "[{}] Failed to allocate descriptor sets! [pool={}]\n",
                string_VkResult(result),
                reinterpret_cast<void*>(descriptorPool)
            );
        }

        // Return
        return descriptorSets;
    }

    void Context::CreateInstance(SDL_Window* window)
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
            .apiVersion         = VULKAN_API_VERSION
        };

        // Get extensions
        auto extensions = m_extensions.LoadInstanceExtensions(window);
        // Create extensions string
        std::string extDbg = fmt::format("Loaded vulkan instance extensions: {}\n", extensions.size());
        for (auto&& extension : extensions)
        {
            // Add
            extDbg.append(fmt::format("- {}\n", extension));
        }
        // Log
        Logger::Debug("{}", extDbg);

        #ifdef ENGINE_DEBUG
        // Request validation layers
        m_layers = Vk::ValidationLayers(VALIDATION_LAYERS);
        #endif

        // Instance creation info
        VkInstanceCreateInfo createInfo =
        {
            .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            #ifdef ENGINE_DEBUG
            .pNext                   = reinterpret_cast<VkDebugUtilsMessengerCreateInfoEXT*>(&m_layers.messengerInfo),
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
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
        {
            // Vulkan initialisation failed, abort
            Logger::Error("{}\n", "Failed to initialise vulkan instance!");
        }

        // Log
        Logger::Info("Successfully initialised Vulkan instance! [handle={}]\n", reinterpret_cast<void*>(instance));

        // Load extensions
        m_extensions.LoadInstanceFunctions(instance);
    }

    void Context::CreateSurface(SDL_Window* window)
    {
        // Create surface using SDL
        if (SDL_Vulkan_CreateSurface(window, instance, &surface) != SDL_TRUE)
        {
            // Log
            Logger::Error
            (
                "Failed to create surface! [window={}] [instance={}]\n",
                reinterpret_cast<void*>(window),
                reinterpret_cast<void*>(instance)
            );
        }

        // Log
        Logger::Info("Initialised window surface! [handle={}]\n", reinterpret_cast<void*>(surface));
    }

    void Context::PickPhysicalDevice()
    {
        // Get device count
        u32 deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        // Make sure we have at least one GPU that supports Vulkan
        if (deviceCount == 0)
        {
            // Log
            Logger::Error("No physical devices found! [instance = {}]\n", reinterpret_cast<void*>(instance));
        }

        // Physical devices
        auto devices = std::vector<VkPhysicalDevice>(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        // Debug list
        std::string dbgPhyDevices = fmt::format("Available physical devices: {}\n", deviceCount);
        // Devices data
        auto properties = std::unordered_map<VkPhysicalDevice, VkPhysicalDeviceProperties2>(deviceCount);
        auto features   = std::unordered_map<VkPhysicalDevice, VkPhysicalDeviceFeatures2>(deviceCount);
        // Device scores
        auto scores = std::map<usize, VkPhysicalDevice>{};

        // Get information about physical devices
        for (const auto& currentDevice : devices)
        {
            // Property set
            VkPhysicalDeviceProperties2 propertySet =
            {
                .sType      = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                .pNext      = nullptr,
                .properties = {}
            };

            // Feature set
            VkPhysicalDeviceFeatures2 featureSet =
            {
                .sType    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                .pNext    = nullptr,
                .features = {}
            };

            // Get data
            vkGetPhysicalDeviceProperties2(currentDevice, &propertySet);
            vkGetPhysicalDeviceFeatures2(currentDevice, &featureSet);

            // Load data
            properties.emplace(currentDevice, propertySet);
            features.emplace(currentDevice, featureSet);

            // Add to string
            dbgPhyDevices.append(fmt::format
            (
                "- {} [Driver Version: {}] [API Version: {}] [ID = {}] \n",
                propertySet.properties.deviceName,
                propertySet.properties.driverVersion,
                propertySet.properties.apiVersion,
                propertySet.properties.deviceID
            ));

            // Add score
            scores.emplace(CalculateScore(currentDevice, propertySet, featureSet), currentDevice);
        }

        // Log string
        Logger::Debug("{}", dbgPhyDevices);

        // Best GPU => Highest score
        auto [highestScore, bestDevice] = *scores.rbegin();
        // Make sure device is valid
        if (highestScore == 0)
        {
            // Log
            Logger::Error("Failed to find any suitable physical device!");
        }
        // Set GPU
        physicalDevice = bestDevice;

        // Get memory properties
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemProperties);
        // Get limits
        physicalDeviceLimits = properties[physicalDevice].properties.limits;

        // Log
        Logger::Info("Selecting GPU: {}\n", properties[physicalDevice].properties.deviceName);
    }

    usize Context::CalculateScore
    (
        VkPhysicalDevice logicalDevice,
        VkPhysicalDeviceProperties2& propertySet,
        VkPhysicalDeviceFeatures2& featureSet
    )
    {
        // Get queue
        auto queue = QueueFamilyIndices(logicalDevice, surface);

        // Calculate score parts
        usize discreteGPU = (propertySet.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) ? 10000 : 100;

        // Calculate score multipliers
        bool isQueueValid  = queue.IsComplete();
        bool hasExtensions = m_extensions.CheckDeviceExtensionSupport(logicalDevice, REQUIRED_EXTENSIONS);
        bool hasAnisotropy = featureSet.features.samplerAnisotropy;
        bool hasWireframe  = featureSet.features.fillModeNonSolid;
        bool multiViewport = featureSet.features.multiViewport;

        // Need extensions to calculate these
        bool isSwapChainAdequate = true;

        // Calculate
        if (hasExtensions)
        {
            // Make sure we have valid presentation and surface formats
            auto swapChainInfo = Vk::SwapchainInfo(logicalDevice, surface);
            isSwapChainAdequate = !(swapChainInfo.formats.empty() || swapChainInfo.presentModes.empty());
        }

        // Calculate score
        return hasExtensions * isQueueValid * isSwapChainAdequate * hasAnisotropy * hasWireframe * multiViewport * discreteGPU;
    }

    void Context::CreateLogicalDevice()
    {
        // Get queue
        queueFamilies = QueueFamilyIndices(physicalDevice, surface);

        // Queue priority
        constexpr f32 queuePriority = 1.0f;

        // Queues data
        auto uniqueQueueFamilies = queueFamilies.GetUniqueFamilies();
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

        // Create queue creation data
        for (auto queueFamily : uniqueQueueFamilies)
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
        deviceFeatures.samplerAnisotropy = VK_TRUE;
        deviceFeatures.fillModeNonSolid  = VK_TRUE;
        deviceFeatures.multiViewport     = VK_TRUE;

        // Logical device creation info
        VkDeviceCreateInfo createInfo =
        {
            .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .queueCreateInfoCount    = static_cast<u32>(queueCreateInfos.size()),
            .pQueueCreateInfos       = queueCreateInfos.data(),
        #ifdef ENGINE_DEBUG
            .enabledLayerCount       = static_cast<u32>(VALIDATION_LAYERS.size()),
            .ppEnabledLayerNames     = VALIDATION_LAYERS.data(),
        #else
            .enabledLayerCount       = 0,
            .ppEnabledLayerNames     = nullptr,
        #endif
            .enabledExtensionCount   = static_cast<u32>(REQUIRED_EXTENSIONS.size()),
            .ppEnabledExtensionNames = REQUIRED_EXTENSIONS.data(),
            .pEnabledFeatures        = &deviceFeatures
        };

        // Create device
        if (vkCreateDevice(
                physicalDevice,
                &createInfo,
                nullptr,
                &device
            ) != VK_SUCCESS)
        {
            // Log
            Logger::Error("Failed to create logical device! [Physical Device = {}]\n", reinterpret_cast<void*>(physicalDevice));
        }

        // Get functions
        m_extensions.LoadDeviceFunctions(device);

        // Log
        Logger::Info("Successfully created vulkan logical device! [handle={}]\n", reinterpret_cast<void*>(device));

        // Get queue
        vkGetDeviceQueue
        (
            device,
            queueFamilies.graphicsFamily.value_or(0),
            0,
            &graphicsQueue
        );
    }

    void Context::CreateCommandPool()
    {
        // Command pool info
        VkCommandPoolCreateInfo createInfo =
        {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext            = nullptr,
            .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = queueFamilies.graphicsFamily.value_or(0)
        };

        // Create command pool
        if (vkCreateCommandPool(
                device,
                &createInfo,
                nullptr,
                &commandPool
            ) != VK_SUCCESS)
        {
            // Log
            Logger::Error("Failed to create command pool! [device={}]\n", reinterpret_cast<void*>(device));
        }

        // Log
        Logger::Info("Created command pool! [handle={}]\n", reinterpret_cast<void*>(commandPool));

        // Add to deletion queue
        m_deletionQueue.PushDeletor([this] ()
        {
            // Destroy command pool
            vkDestroyCommandPool(device, commandPool, nullptr);
        });
    }

    void Context::CreateDescriptorPool()
    {
        // Creation info
        VkDescriptorPoolCreateInfo poolCreateInfo =
        {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext         = nullptr,
            .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT, // Only for you ImGui
            .maxSets       = static_cast<u32>(Vk::GetDescriptorPoolSize()),
            .poolSizeCount = Vk::DESCRIPTOR_POOL_SIZES.size(),
            .pPoolSizes    = Vk::DESCRIPTOR_POOL_SIZES.data()
        };

        // Create
        if (vkCreateDescriptorPool(
                device,
                &poolCreateInfo,
                nullptr,
                &descriptorPool
            ) != VK_SUCCESS)
        {
            // Log
            Logger::Error("Failed to create descriptor pool! [device={}]\n", reinterpret_cast<void*>(device));
        }

        // Log
        Logger::Info("Created descriptor pool! [handle={}]\n", reinterpret_cast<void*>(descriptorPool));

        // Add to deletion queue
        m_deletionQueue.PushDeletor([this] ()
        {
            // Destroy descriptor pool
            vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        });
    }

    void Context::CreateAllocator()
    {
        // Allocator info
        VmaAllocatorCreateInfo createInfo =
        {
            .flags                       = 0,
            .physicalDevice              = physicalDevice,
            .device                      = device,
            .preferredLargeHeapBlockSize = 0,
            .pAllocationCallbacks        = nullptr,
            .pDeviceMemoryCallbacks      = nullptr,
            .pHeapSizeLimit              = nullptr,
            .pVulkanFunctions            = nullptr,
            .instance                    = instance,
            .vulkanApiVersion            = VULKAN_API_VERSION
            #if VMA_EXTERNAL_MEMORY
            ,.pTypeExternalMemoryHandleTypes = nullptr
            #endif
        };

        // Create allocator
        vmaCreateAllocator(&createInfo, &allocator);

        // Log
        Logger::Info("Created vulkan memory allocator! [handle={}]\n", std::bit_cast<void*>(allocator));

        // Add to deletion queue
        m_deletionQueue.PushDeletor([this] ()
        {
            // Destroy allocator
            vmaDestroyAllocator(allocator);
        });
    }

    void Context::Destroy()
    {
        // Flush deletion queue
        m_deletionQueue.FlushQueue();

        // Destroy surface
        vkDestroySurfaceKHR(instance, surface, nullptr);
        // Destroy logical device
        vkDestroyDevice(device, nullptr);

        #ifdef ENGINE_DEBUG
        // Destroy validation layers
        m_layers.Destroy(instance);
        #endif

        // Destroy extensions
        m_extensions.Destroy();
        // Destroy vulkan instance
        vkDestroyInstance(instance, nullptr);

        // Log
        Logger::Info("{}\n", "Destroyed vulkan context!");
    }
}