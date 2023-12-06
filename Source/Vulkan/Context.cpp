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
    #ifdef ENGINE_DEBUG
    // Layers
    constexpr std::array<const char*, 1> VALIDATION_LAYERS = {"VK_LAYER_KHRONOS_validation"};
    #endif
    // Required device extensions
    constexpr std::array<const char*, 1> REQUIRED_EXTENSIONS = {"VK_KHR_swapchain"};

    Context::Context(const std::shared_ptr<Engine::Window>& window)
    {
        // Create vulkan instance
        CreateVKInstance(window->handle);

        #ifdef ENGINE_DEBUG
        // Load validation layers
        if (m_layers.SetupMessenger(vkInstance) != VK_SUCCESS)
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

        // Creates command buffer
        CreateCommandBuffers();
        // Create sync objects
        CreateSyncObjects();

        // Log
        Logger::Info("{}\n", "Initialised vulkan context!");
    }

    std::vector<VkCommandBuffer> Context::AllocateCommandBuffers(u32 count, VkCommandBufferLevel level)
    {
        // Command buffer allocation data
        VkCommandBufferAllocateInfo allocInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext              = nullptr,
            .commandPool        = m_commandPool,
            .level              = level,
            .commandBufferCount = count
        };
        // Create space
        auto cmdBuffers = std::vector<VkCommandBuffer>(count);
        // Allocate
        vkAllocateCommandBuffers(device, &allocInfo, cmdBuffers.data());
        // Return
        return cmdBuffers;
    }

    void Context::FreeCommandBuffers(const std::span<const VkCommandBuffer> cmdBuffers)
    {
        // Free
        vkFreeCommandBuffers
        (
            device,
            m_commandPool,
            static_cast<u32>(cmdBuffers.size()),
            cmdBuffers.data()
        );
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
            Logger::Error("[{}] Failed to allocate descriptor sets! [pool={}]\n", string_VkResult(result), reinterpret_cast<void*>(descriptorPool));
        }

        // Return
        return descriptorSets;
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
        if (vkCreateInstance(&createInfo, nullptr, &vkInstance) != VK_SUCCESS)
        {
            // Vulkan initialisation failed, abort
            Logger::Error("{}\n", "Failed to initialise vulkan instance!");
        }

        // Log
        Logger::Info("Successfully initialised Vulkan instance! [handle={}]\n", reinterpret_cast<void*>(vkInstance));

        // Load extensions
        m_extensions.LoadInstanceFunctions(vkInstance);
    }

    void Context::CreateSurface(SDL_Window* window)
    {
        // Create surface using SDL
        if (SDL_Vulkan_CreateSurface(window, vkInstance, &surface) != SDL_TRUE)
        {
            // Log
            Logger::Error
            (
                "Failed to create surface! [window={}] [instance={}]\n",
                reinterpret_cast<void*>(window),
                reinterpret_cast<void*>(vkInstance)
            );
        }

        // Log
        Logger::Info("Initialised vulkan surface! [handle={}]\n", reinterpret_cast<void*>(surface));
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
            Logger::Error("No physical devices found! [instance = {}]\n", reinterpret_cast<void*>(vkInstance));
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
        for (const auto& currentDevice : devices)
        {
            // Get data
            VkPhysicalDeviceProperties propertySet;
            vkGetPhysicalDeviceProperties(currentDevice, &propertySet);
            VkPhysicalDeviceFeatures featureSet;
            vkGetPhysicalDeviceFeatures(currentDevice, &featureSet);

            // Load data
            properties.emplace(currentDevice, propertySet);
            features.emplace(currentDevice, featureSet);

            // Add to string
            dbgPhyDevices.append(fmt::format
            (
                "- {} [Driver Version: {}] [API Version: {}] [ID = {}] \n",
                propertySet.deviceName,
                propertySet.driverVersion,
                propertySet.apiVersion,
                propertySet.deviceID
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
            Logger::Error("Failed to load physical device! [name=\"{}\"]", properties[bestDevice].deviceName);
        }
        // Set GPU
        physicalDevice = bestDevice;

        // Get memory properties
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &phyMemProperties);

        // Log
        Logger::Info("Selecting GPU: {}\n", properties[physicalDevice].deviceName);
    }

    usize Context::CalculateScore
    (
        VkPhysicalDevice logicalDevice,
        VkPhysicalDeviceProperties& propertySet,
        VkPhysicalDeviceFeatures& featureSet
    )
    {
        // Get queue
        auto queue = QueueFamilyIndices(logicalDevice, surface);

        // Calculate score parts
        usize discreteGPU = (propertySet.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) ? 10000 : 100;

        // Calculate score multipliers
        bool isQueueValid  = queue.IsComplete();
        bool hasExtensions = m_extensions.CheckDeviceExtensionSupport(logicalDevice, REQUIRED_EXTENSIONS);
        bool hasAnisotropy = featureSet.samplerAnisotropy;

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
        return hasExtensions * isQueueValid * isSwapChainAdequate * hasAnisotropy * discreteGPU;
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
            queueFamilies.graphicsFamily.value(),
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
            .queueFamilyIndex = queueFamilies.graphicsFamily.value()
        };

        // Create command pool
        if (vkCreateCommandPool(
                device,
                &createInfo,
                nullptr,
                &m_commandPool
            ) != VK_SUCCESS)
        {
            // Log
            Logger::Error("Failed to create command pool! [device={}]\n", reinterpret_cast<void*>(device));
        }

        // Log
        Logger::Info("Created command pool! [handle={}]\n", reinterpret_cast<void*>(m_commandPool));

        // Add to deletion queue
        m_deletionQueue.PushDeletor([this] ()
        {
            // Destroy command pool
            vkDestroyCommandPool(device, m_commandPool, nullptr);
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

    void Context::CreateCommandBuffers()
    {
        // Allocate command buffers
        auto _commandBuffers = AllocateCommandBuffers(commandBuffers.size(), VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        // Convert
        std::move
        (
            _commandBuffers.begin(),
            _commandBuffers.begin() + static_cast<ssize>(commandBuffers.size()),
            commandBuffers.begin()
        );
        // Log
        Logger::Info("{}\n", "Created command buffers!");
    }

    void Context::CreateSyncObjects()
    {
        // Semaphore info
        VkSemaphoreCreateInfo semaphoreInfo =
        {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0
        };

        // Fence info
        VkFenceCreateInfo fenceInfo =
        {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };

        // For each frame in flight
        for (usize i = 0; i < FRAMES_IN_FLIGHT; ++i)
        {
            if
            (
                vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                    &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                    &renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(device, &fenceInfo, nullptr,
                    &inFlightFences[i]) != VK_SUCCESS
            )
            {
                // Log
                Logger::Error("{}\n", "Failed to create sync objects!");
            }
        }

        // Log
        Logger::Info("{}\n", "Created synchronisation objects!");

        // Add to deletion queue
        m_deletionQueue.PushDeletor([this] ()
        {
            // Destroy sync objects
            for (usize i = 0; i < FRAMES_IN_FLIGHT; ++i)
            {
                // Destroy semaphores
                vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
                vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
                // Destroy fences
                vkDestroyFence(device, inFlightFences[i], nullptr);
            }
        });
    }

    void Context::Destroy()
    {
        // Flush deletion queue
        m_deletionQueue.FlushQueue();

        // Destroy surface
        vkDestroySurfaceKHR(vkInstance, surface, nullptr);
        // Destroy logical device
        vkDestroyDevice(device, nullptr);

        #ifdef ENGINE_DEBUG
        // Destroy validation layers
        m_layers.DestroyMessenger(vkInstance);
        #endif

        // Destroy extensions
        m_extensions.Destroy();
        // Destroy vulkan instance
        vkDestroyInstance(vkInstance, nullptr);

        // Log
        Logger::Info("{}\n", "Destroyed vulkan context!");
    }
}