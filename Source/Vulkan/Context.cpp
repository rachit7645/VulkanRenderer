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
#include <array>
#include <vector>
#include <vulkan/vk_enum_string_helper.h>

#include "Extensions.h"
#include "SwapchainInfo.h"
#include "Util.h"
#include "Constants.h"
#include "Util/Log.h"

namespace Vk
{
    // Required validation layers
    #ifdef ENGINE_DEBUG
    constexpr std::array VALIDATION_LAYERS = {"VK_LAYER_KHRONOS_validation", "VK_LAYER_KHRONOS_synchronization2"};
    #endif

    // Required device extensions
    constexpr std::array REQUIRED_EXTENSIONS = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

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

    void Context::CreateInstance(SDL_Window* window)
    {
        constexpr VkApplicationInfo appInfo =
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

        const VkInstanceCreateInfo createInfo =
        {
            .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            #ifdef ENGINE_DEBUG
            .pNext                   = &m_layers.messengerInfo,
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

        m_deletionQueue.PushDeletor([this] ()
        {
            vkDestroySurfaceKHR(instance, surface, nullptr);
        });
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
            VkPhysicalDeviceProperties2 propertySet = {};
            propertySet.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

            VkPhysicalDeviceVulkan12Features vk12Features = {};
            vk12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

            VkPhysicalDeviceVulkan13Features vk13Features = {};
            vk13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
            vk13Features.pNext = &vk12Features;

            VkPhysicalDeviceFeatures2 featureSet = {};
            featureSet.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            featureSet.pNext = &vk13Features;

            vkGetPhysicalDeviceProperties2(currentDevice, &propertySet);
            vkGetPhysicalDeviceFeatures2(currentDevice, &featureSet);

            properties.emplace(currentDevice, propertySet);
            features.emplace(currentDevice, featureSet);

            scores.emplace(CalculateScore(currentDevice, propertySet, featureSet), currentDevice);
        }

        // Best GPU => Highest score
        const auto [highestScore, bestDevice] = *scores.crbegin();
        // Score = 0 => Required features not supported
        if (highestScore == 0)
        {
            Logger::VulkanError("Failed to find any suitable physical device!");
        }

        physicalDevice = bestDevice;

        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemProperties);
        physicalDeviceLimits = properties[physicalDevice].properties.limits;

        Logger::Info
        (
            "Selecting GPU: {} [{}]\n",
            properties[physicalDevice].properties.deviceName,
            string_VkPhysicalDeviceType(properties[physicalDevice].properties.deviceType)
        );
    }

    usize Context::CalculateScore
    (
        const VkPhysicalDevice phyDevice,
        const VkPhysicalDeviceProperties2& propertySet,
        const VkPhysicalDeviceFeatures2& featureSet
    )
    {
        const auto queue = QueueFamilyIndices(phyDevice, surface);
        const auto vk13Features = static_cast<VkPhysicalDeviceVulkan13Features*>(featureSet.pNext);
        const auto vk12Features = static_cast<VkPhysicalDeviceVulkan12Features*>(vk13Features->pNext);

        // Score parts
        const usize discreteGPU = (propertySet.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) ? 10000 : 100;

        // Standard features
        const bool isQueueValid  = queue.IsComplete();
        const bool hasExtensions = m_extensions.CheckDeviceExtensionSupport(phyDevice, REQUIRED_EXTENSIONS);
        const bool hasAnisotropy = featureSet.features.samplerAnisotropy;
        const bool hasWireframe  = featureSet.features.fillModeNonSolid;

        // Need extensions to calculate these
        bool isSwapChainAdequate = false;

        if (hasExtensions)
        {
            const auto swapChainInfo = Vk::SwapchainInfo(phyDevice, surface);
            isSwapChainAdequate = !(swapChainInfo.formats.empty() || swapChainInfo.presentModes.empty());
        }

        // Vulkan 1.2 features
        const bool hasBDA          = vk12Features->bufferDeviceAddress;
        const bool hasScalarLayout = vk12Features->scalarBlockLayout;

        // Vulkan 1.3 features
        const bool hasSync2        = vk13Features->synchronization2;
        const bool hasDynRender    = vk13Features->dynamicRendering;
        const bool hasMaintenance4 = vk13Features->maintenance4;

        return isQueueValid    * hasExtensions * isSwapChainAdequate *
               hasAnisotropy   * hasWireframe  * hasBDA              *
               hasScalarLayout * hasSync2      * hasDynRender        *
               hasMaintenance4 * discreteGPU;
    }

    void Context::CreateLogicalDevice()
    {
        queueFamilies = QueueFamilyIndices(physicalDevice, surface);

        auto uniqueQueueFamilies = queueFamilies.GetUniqueFamilies();

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        queueCreateInfos.reserve(uniqueQueueFamilies.size());

        constexpr f32 QUEUE_PRIORITY = 1.0f;

        for (auto queueFamily : uniqueQueueFamilies)
        {
            queueCreateInfos.emplace_back(VkDeviceQueueCreateInfo{
                .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext            = nullptr,
                .flags            = 0,
                .queueFamilyIndex = queueFamily,
                .queueCount       = 1,
                .pQueuePriorities = &QUEUE_PRIORITY
            });
        }

        // Add required Vulkan 1.2 features here
        VkPhysicalDeviceVulkan12Features vk12Features = {};
        vk12Features.sType               = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        vk12Features.pNext               = nullptr;
        vk12Features.bufferDeviceAddress = VK_TRUE;
        vk12Features.scalarBlockLayout   = VK_TRUE;

        // Add required Vulkan 1.3 features here
        VkPhysicalDeviceVulkan13Features vk13Features = {};
        vk13Features.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        vk13Features.pNext            = &vk12Features;
        vk13Features.synchronization2 = VK_TRUE;
        vk13Features.dynamicRendering = VK_TRUE;
        vk13Features.maintenance4     = VK_TRUE;

        // Add required features here
        VkPhysicalDeviceFeatures2 deviceFeatures = {};
        deviceFeatures.sType                      = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures.pNext                      = &vk13Features;
        deviceFeatures.features.samplerAnisotropy = VK_TRUE;
        deviceFeatures.features.fillModeNonSolid  = VK_TRUE;

        const VkDeviceCreateInfo createInfo =
        {
            .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext                   = &deviceFeatures,
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
            .pEnabledFeatures        = nullptr
        };

        Vk::CheckResult(vkCreateDevice(
            physicalDevice,
            &createInfo,
            nullptr,
            &device),
            "Failed to create logical device!"
        );

        m_extensions.LoadDeviceFunctions(device);

        Logger::Info("Created logical device! [handle={}]\n", std::bit_cast<void*>(device));

        // We assume that this graphics queue also has transfer capabilities
        // We already know that it has presentation capabilities
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
        const VkCommandPoolCreateInfo createInfo =
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
            Vk::CheckResult(vkResetCommandPool(device, commandPool, 0), "Failed to reset command pool!");
            vkDestroyCommandPool(device, commandPool, nullptr);
        });
    }

    void Context::CreateDescriptorPool()
    {
        constexpr VkDescriptorPoolSize samplerPoolSize =
        {
            .type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = (1 << 4) * FRAMES_IN_FLIGHT
        };

        const VkDescriptorPoolCreateInfo poolCreateInfo =
        {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext         = nullptr,
            .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
            .maxSets       = static_cast<u32>(samplerPoolSize.descriptorCount),
            .poolSizeCount = 1,
            .pPoolSizes    = &samplerPoolSize
        };

        Vk::CheckResult(vkCreateDescriptorPool(
            device,
            &poolCreateInfo,
            nullptr,
            &imguiDescriptorPool),
            "Failed to create ImGui descriptor pool!"
        );

        Logger::Info("Created ImGui descriptor pool! [handle={}]\n", std::bit_cast<void*>(imguiDescriptorPool));

        descriptorCache = Vk::DescriptorCache(device);

        m_deletionQueue.PushDeletor([this] ()
        {
            vkDestroyDescriptorPool(device, imguiDescriptorPool, nullptr);
            descriptorCache.Destroy(device);
        });
    }

    void Context::CreateAllocator()
    {
        const VmaAllocatorCreateInfo createInfo =
        {
            .flags                       = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
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

        vkDestroyDevice(device, nullptr);

        #ifdef ENGINE_DEBUG
        m_layers.Destroy(instance);
        #endif

        m_extensions.Destroy();

        vkDestroyInstance(instance, nullptr);

        Logger::Info("{}\n", "Destroyed vulkan context!");
    }
}