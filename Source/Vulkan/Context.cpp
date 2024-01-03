/*
 *    Copyright 2023 - 2024 Rachit Khandelwal
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
#include "Util.h"

namespace Vk
{
    // Constants
    constexpr auto VULKAN_API_VERSION = VK_API_VERSION_1_3;

    // Required validation layers
    #ifdef ENGINE_DEBUG
    constexpr std::array<const char*, 2> VALIDATION_LAYERS = {"VK_LAYER_KHRONOS_validation", "VK_LAYER_KHRONOS_synchronization2"};
    #endif

    // Required device extensions
    #ifdef ENGINE_DEBUG
    constexpr std::array<const char*, 1> REQUIRED_EXTENSIONS = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    #else
    constexpr std::array<const char*, 1> REQUIRED_EXTENSIONS = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    #endif

    Context::Context(const std::shared_ptr<Engine::Window>& window)
    {
        CreateInstance(window->handle);

        CreateSurface(window->handle);

        PickPhysicalDevice();
        CreateLogicalDevice();

        CreateCommandPool();
        CreateDescriptorPool();
        CreateAllocator();

        Logger::Info("{}\n", "Initialised vulkan context!");
    }

    std::vector<VkDescriptorSet> Context::AllocateDescriptorSets(const std::span<VkDescriptorSetLayout> descriptorLayouts)
    {
        VkDescriptorSetAllocateInfo allocInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext              = nullptr,
            .descriptorPool     = descriptorPool,
            .descriptorSetCount = static_cast<u32>(descriptorLayouts.size()),
            .pSetLayouts        = descriptorLayouts.data()
        };

        auto descriptorSets = std::vector<VkDescriptorSet>(descriptorLayouts.size(), VK_NULL_HANDLE);

        Vk::CheckResult(vkAllocateDescriptorSets(
            device,
            &allocInfo,
            descriptorSets.data()),
            "Failed to allocate descriptor sets!"
        );

        // Return
        return descriptorSets;
    }

    void Context::CreateInstance(SDL_Window* window)
    {
        VkApplicationInfo appInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext              = nullptr,
            .pApplicationName   = "Vulkan Renderer",
            .applicationVersion = VK_MAKE_API_VERSION(0, 0, 0, 1),
            .pEngineName        = "Rachit's Engine",
            .engineVersion      = VK_MAKE_API_VERSION(0, 0, 0, 1),
            .apiVersion         = VULKAN_API_VERSION
        };

        auto extensions = m_extensions.LoadInstanceExtensions(window);

        #ifdef ENGINE_DEBUG
        m_layers = Vk::ValidationLayers(VALIDATION_LAYERS);
        #endif

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

        Vk::CheckResult(vkCreateInstance(
            &createInfo,
            nullptr,
            &instance),
            "Failed to initialise vulkan instance!"
        );

        Logger::Info("Successfully initialised Vulkan instance! [handle={}]\n", std::bit_cast<void*>(instance));

        m_extensions.LoadInstanceFunctions(instance);

        #ifdef ENGINE_DEBUG
        m_layers.SetupMessenger(instance);
        #endif
    }

    void Context::CreateSurface(SDL_Window* window)
    {
        if (SDL_Vulkan_CreateSurface(window, instance, &surface) != SDL_TRUE)
        {
            Logger::Error
            (
                "Failed to create surface! [window={}] [instance={}]\n",
                std::bit_cast<void*>(window),
                std::bit_cast<void*>(instance)
            );
        }

        Logger::Info("Initialised window surface! [handle={}]\n", std::bit_cast<void*>(surface));
    }

    void Context::PickPhysicalDevice()
    {
        u32 deviceCount = 0;
        Vk::CheckResult(vkEnumeratePhysicalDevices(
            instance,
            &deviceCount,
            nullptr),
            "Failed to get physical device count!"
        );

        // We need at least one device that supports vulkan
        if (deviceCount == 0)
        {
            Logger::Error("No physical devices found! [instance={}]\n", std::bit_cast<void*>(instance));
        }

        auto devices = std::vector<VkPhysicalDevice>(deviceCount);
        Vk::CheckResult(vkEnumeratePhysicalDevices(
            instance,
            &deviceCount,
            devices.data()),
            "Failed to get physical devices!"
        );

        auto properties = std::unordered_map<VkPhysicalDevice, VkPhysicalDeviceProperties2>(deviceCount);
        auto features   = std::unordered_map<VkPhysicalDevice, VkPhysicalDeviceFeatures2>(deviceCount);
        auto scores     = std::map<usize, VkPhysicalDevice>{};

        for (const auto& currentDevice : devices)
        {
            VkPhysicalDeviceProperties2 propertySet =
            {
                .sType      = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                .pNext      = nullptr,
                .properties = {}
            };

            VkPhysicalDeviceVulkan13Features vk13Features = {};
            vk13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
            vk13Features.pNext = nullptr;

            VkPhysicalDeviceFeatures2 featureSet =
            {
                .sType    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                .pNext    = &vk13Features,
                .features = {}
            };

            vkGetPhysicalDeviceProperties2(currentDevice, &propertySet);
            vkGetPhysicalDeviceFeatures2(currentDevice, &featureSet);

            properties.emplace(currentDevice, propertySet);
            features.emplace(currentDevice, featureSet);

            scores.emplace(CalculateScore(currentDevice, propertySet, featureSet), currentDevice);
        }

        // Best GPU => Highest score
        auto [highestScore, bestDevice] = *scores.rbegin();
        // Score = 0 => Required features not supported
        if (highestScore == 0)
        {
            // Log
            Logger::VulkanError("Failed to find any suitable physical device!");
        }

        physicalDevice = bestDevice;

        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemProperties);
        physicalDeviceLimits = properties[physicalDevice].properties.limits;

        Logger::Info
        (
            "Selecting GPU: {} [{}] [DriverVersion={}] [APIVersion={}]\n",
            properties[physicalDevice].properties.deviceName,
            string_VkPhysicalDeviceType(properties[physicalDevice].properties.deviceType),
            properties[physicalDevice].properties.driverVersion,
            properties[physicalDevice].properties.apiVersion
        );
    }

    usize Context::CalculateScore
    (
        VkPhysicalDevice logicalDevice,
        VkPhysicalDeviceProperties2& propertySet,
        VkPhysicalDeviceFeatures2& featureSet
    )
    {
        auto queue = QueueFamilyIndices(logicalDevice, surface);
        auto vk13Features = reinterpret_cast<VkPhysicalDeviceVulkan13Features*>(featureSet.pNext);

        // Calculate score parts
        usize discreteGPU = (propertySet.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) ? 10000 : 100;

        // Calculate score multipliers
        bool isQueueValid  = queue.IsComplete();
        bool hasExtensions = m_extensions.CheckDeviceExtensionSupport(logicalDevice, REQUIRED_EXTENSIONS);
        bool hasAnisotropy = featureSet.features.samplerAnisotropy;
        bool hasWireframe  = featureSet.features.fillModeNonSolid;
        bool multiViewport = featureSet.features.multiViewport;
        bool hasSync2      = vk13Features->synchronization2;
        bool hasDynRender  = vk13Features->dynamicRendering;

        // Need extensions to calculate these
        bool isSwapChainAdequate = false;

        if (hasExtensions)
        {
            auto swapChainInfo = Vk::SwapchainInfo(logicalDevice, surface);
            isSwapChainAdequate = !(swapChainInfo.formats.empty() || swapChainInfo.presentModes.empty());
        }

        return hasExtensions * isQueueValid * isSwapChainAdequate *
               hasAnisotropy * hasWireframe * multiViewport *
               hasSync2      * hasDynRender * discreteGPU;
    }

    void Context::CreateLogicalDevice()
    {
        queueFamilies = QueueFamilyIndices(physicalDevice, surface);

        auto uniqueQueueFamilies = queueFamilies.GetUniqueFamilies();
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

        constexpr f32 QUEUE_PRIORITY = 1.0f;

        for (auto queueFamily : uniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo queueCreateInfo =
            {
                .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext            = nullptr,
                .flags            = 0,
                .queueFamilyIndex = queueFamily,
                .queueCount       = 1,
                .pQueuePriorities = &QUEUE_PRIORITY
            };

            queueCreateInfos.push_back(queueCreateInfo);
        }

        // Add required features here
        VkPhysicalDeviceFeatures deviceFeatures = {};
        deviceFeatures.samplerAnisotropy = VK_TRUE;
        deviceFeatures.fillModeNonSolid  = VK_TRUE;
        deviceFeatures.multiViewport     = VK_TRUE;

        // Add required Vulkan 1.3 features here
        VkPhysicalDeviceVulkan13Features vk13Features = {};
        vk13Features.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        vk13Features.pNext            = nullptr;
        vk13Features.synchronization2 = VK_TRUE;
        vk13Features.dynamicRendering = VK_TRUE;

        VkDeviceCreateInfo createInfo =
        {
            .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext                   = &vk13Features,
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

        Vk::CheckResult(vkCreateDevice(
            physicalDevice,
            &createInfo,
            nullptr,
            &device),
            "Failed to create logical device!"
        );

        m_extensions.LoadDeviceFunctions(device);

        Logger::Info("Successfully created vulkan logical device! [handle={}]\n", std::bit_cast<void*>(device));

        // We assume that this graphics queue also has transfer capabilities
        // Also we make sure that it has presentation capabilities
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
        VkCommandPoolCreateInfo createInfo =
        {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext            = nullptr,
            .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = queueFamilies.graphicsFamily.value_or(0)
        };

        Vk::CheckResult(vkCreateCommandPool(
            device,
            &createInfo,
            nullptr,
            &commandPool),
            "Failed to create command pool!"
        );

        Logger::Info("Created command pool! [handle={}]\n", std::bit_cast<void*>(commandPool));

        m_deletionQueue.PushDeletor([this] ()
        {
            vkDestroyCommandPool(device, commandPool, nullptr);
        });
    }

    void Context::CreateDescriptorPool()
    {
        VkDescriptorPoolCreateInfo poolCreateInfo =
        {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext         = nullptr,
            .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            .maxSets       = static_cast<u32>(Vk::GetDescriptorPoolSize()),
            .poolSizeCount = Vk::DESCRIPTOR_POOL_SIZES.size(),
            .pPoolSizes    = Vk::DESCRIPTOR_POOL_SIZES.data()
        };

        Vk::CheckResult(vkCreateDescriptorPool(
            device,
            &poolCreateInfo,
            nullptr,
            &descriptorPool),
            "Failed to create descriptor pool!"
        );

        Logger::Info("Created descriptor pool! [handle={}]\n", std::bit_cast<void*>(descriptorPool));

        m_deletionQueue.PushDeletor([this] ()
        {
            vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        });
    }

    void Context::CreateAllocator()
    {
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

        Vk::CheckResult(vmaCreateAllocator(&createInfo, &allocator), "Failed to create allocator!");

        Logger::Info("Created vulkan memory allocator! [handle={}]\n", std::bit_cast<void*>(allocator));

        m_deletionQueue.PushDeletor([this] ()
        {
            vmaDestroyAllocator(allocator);
        });
    }

    void Context::Destroy()
    {
        m_deletionQueue.FlushQueue();

        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyDevice(device, nullptr);

        #ifdef ENGINE_DEBUG
        m_layers.Destroy(instance);
        #endif

        m_extensions.Destroy();
        vkDestroyInstance(instance, nullptr);

        Logger::Info("{}\n", "Destroyed vulkan context!");
    }
}