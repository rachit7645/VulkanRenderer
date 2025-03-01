/*
 * Copyright (c) 2023 - 2025 Rachit
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Context.h"

#include <map>
#include <unordered_map>
#include <array>
#include <vector>
#include <vulkan/vk_enum_string_helper.h>
#include <volk/volk.h>

#include "Extensions.h"
#include "SwapchainInfo.h"
#include "Util.h"
#include "Constants.h"
#include "DebugUtils.h"
#include "Chain.h"
#include "Util/Log.h"

namespace Vk
{
    // Required validation layers
    #ifdef ENGINE_ENABLE_VALIDATION
    constexpr std::array VALIDATION_LAYERS = {"VK_LAYER_KHRONOS_validation", "VK_LAYER_KHRONOS_synchronization2"};
    #endif

    // Required instance extensions
    constexpr std::array REQUIRED_INSTANCE_EXTENSIONS =
    {
        VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
        VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME,
        #ifdef ENGINE_DEBUG
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
        #endif
    };

    // Required device extensions
    constexpr std::array REQUIRED_DEVICE_EXTENSIONS =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
        VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME,
        #ifdef ENGINE_DEBUG
        VK_KHR_SHADER_RELAXED_EXTENDED_INSTRUCTION_EXTENSION_NAME
        #endif
    };

    Context::Context(SDL_Window* window)
    {
        Vk::CheckResult(volkInitialize(), "Failed to initialize volk!");

        CreateInstance();

        CreateSurface(window);

        PickPhysicalDevice();
        CreateLogicalDevice();

        CreateCommandPool();
        CreateAllocator();

        AddDebugNames();

        Logger::Info("{}\n", "Initialised vulkan context!");
    }

    void Context::CreateInstance()
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

        auto extensions = Vk::LoadInstanceExtensions(REQUIRED_INSTANCE_EXTENSIONS);

        #ifdef ENGINE_ENABLE_VALIDATION
        m_layers = Vk::ValidationLayers(VALIDATION_LAYERS);
        #endif

        const VkInstanceCreateInfo createInfo =
        {
            .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            #ifdef ENGINE_ENABLE_VALIDATION
            .pNext                   = &m_layers.messengerInfo,
            #else
            .pNext                   = nullptr,
            #endif
            .flags                   = 0,
            .pApplicationInfo        = &appInfo,
            #ifdef ENGINE_ENABLE_VALIDATION
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

        volkLoadInstanceOnly(instance);

        #ifdef ENGINE_ENABLE_VALIDATION
        m_layers.SetupMessenger(instance);
        #endif
    }

    void Context::CreateSurface(SDL_Window* window)
    {
        if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface))
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

            VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT swapchainMaintenanceFeatures = {};
            swapchainMaintenanceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT;

            VkPhysicalDeviceVulkan11Features vk11Features = {};
            vk11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
            vk11Features.pNext = &swapchainMaintenanceFeatures;

            VkPhysicalDeviceVulkan12Features vk12Features = {};
            vk12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
            vk12Features.pNext = &vk11Features;

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

        physicalDevice       = bestDevice;
        physicalDeviceLimits = properties[physicalDevice].properties.limits;

        Logger::Info
        (
            "Selecting GPU: {} [Type={}] [Driver Version={}]\n",
            properties[physicalDevice].properties.deviceName,
            string_VkPhysicalDeviceType(properties[physicalDevice].properties.deviceType),
            properties[physicalDevice].properties.driverVersion
        );
    }

    usize Context::CalculateScore
    (
        VkPhysicalDevice phyDevice,
        const VkPhysicalDeviceProperties2& propertySet,
        const VkPhysicalDeviceFeatures2& featureSet
    )
    {
        const auto queues = QueueFamilyIndices(phyDevice, surface);

        const auto vk13Features                 = Vk::FindStructureInChain<VkPhysicalDeviceVulkan13Features>(featureSet.pNext);
        const auto vk12Features                 = Vk::FindStructureInChain<VkPhysicalDeviceVulkan12Features>(featureSet.pNext);
        const auto vk11Features                 = Vk::FindStructureInChain<VkPhysicalDeviceVulkan11Features>(featureSet.pNext);
        const auto swapchainMaintenanceFeatures = Vk::FindStructureInChain<VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT>(featureSet.pNext);

        // Score parts
        const usize discreteGPU = (propertySet.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) ? 10000 : 100;

        // Requirements
        const bool areQueuesValid = queues.IsComplete();
        const bool hasExtensions  = Vk::CheckDeviceExtensionSupport(phyDevice, REQUIRED_DEVICE_EXTENSIONS);

        // Need extensions to calculate these
        bool isSwapChainAdequate     = false;
        bool hasSwapchainMaintenance = false;

        if (hasExtensions)
        {
            const auto swapChainInfo = Vk::SwapchainInfo(phyDevice, surface);

            isSwapChainAdequate     = !(swapChainInfo.formats.empty() || swapChainInfo.presentModes.empty());
            hasSwapchainMaintenance = swapchainMaintenanceFeatures->swapchainMaintenance1;
        }

        // Standard features
        const bool hasAnisotropy        = featureSet.features.samplerAnisotropy;
        const bool hasWireframe         = featureSet.features.fillModeNonSolid;
        const bool hasMultiDrawIndirect = featureSet.features.multiDrawIndirect;
        const bool hasBC                = featureSet.features.textureCompressionBC;
        const bool hasImageCubeArray    = featureSet.features.imageCubeArray;
        const bool hasDepthClamp        = featureSet.features.depthClamp;

        // Vulkan 1.1 features
        const bool hasShaderDrawParameters = vk11Features->shaderDrawParameters;
        const bool hasMultiView            = vk11Features->multiview;

        // Vulkan 1.2 features
        const bool hasBDA                         = vk12Features->bufferDeviceAddress;
        const bool hasScalarLayout                = vk12Features->scalarBlockLayout;
        const bool hasDescriptorIndexing          = vk12Features->descriptorIndexing;
        const bool hasNonUniformIndexing          = vk12Features->shaderSampledImageArrayNonUniformIndexing;
        const bool hasRuntimeDescriptorArray      = vk12Features->runtimeDescriptorArray;
        const bool hasPartiallyBoundDescriptors   = vk12Features->descriptorBindingPartiallyBound;
        const bool hasSampledImageUpdateAfterBind = vk12Features->descriptorBindingSampledImageUpdateAfterBind;
        const bool hasUpdateUnusedWhilePending    = vk12Features->descriptorBindingUpdateUnusedWhilePending;
        const bool hasDrawIndirectCount           = vk12Features->drawIndirectCount;

        // Vulkan 1.3 features
        const bool hasSync2        = vk13Features->synchronization2;
        const bool hasDynRender    = vk13Features->dynamicRendering;
        const bool hasMaintenance4 = vk13Features->maintenance4;

        const bool required   = areQueuesValid && hasExtensions;
        const bool standard   = hasAnisotropy && hasWireframe && hasMultiDrawIndirect && hasBC && hasImageCubeArray && hasDepthClamp;
        const bool extensions = isSwapChainAdequate && hasSwapchainMaintenance;
        const bool vk11       = hasShaderDrawParameters && hasMultiView;
        const bool vk12       = hasBDA && hasScalarLayout && hasDescriptorIndexing && hasNonUniformIndexing &&
                                hasRuntimeDescriptorArray && hasPartiallyBoundDescriptors &&
                                hasSampledImageUpdateAfterBind && hasUpdateUnusedWhilePending && hasDrawIndirectCount;
        const bool vk13       = hasSync2 && hasDynRender && hasMaintenance4;

        return (required && standard && extensions && vk11 && vk12 && vk13) * discreteGPU;
    }

    void Context::CreateLogicalDevice()
    {
        queueFamilies = QueueFamilyIndices(physicalDevice, surface);

        const auto uniqueQueueFamilies = queueFamilies.GetUniqueFamilies();

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        queueCreateInfos.reserve(uniqueQueueFamilies.size());

        constexpr f32 QUEUE_PRIORITY = 1.0f;

        for (const auto queueFamily : uniqueQueueFamilies)
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

        // Swapchain Maintenance
        VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT swapchainMaintenanceFeatures = {};
        swapchainMaintenanceFeatures.sType                 = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT;
        swapchainMaintenanceFeatures.pNext                 = nullptr;
        swapchainMaintenanceFeatures.swapchainMaintenance1 = VK_TRUE;

        // Add required Vulkan 1.1 features here
        VkPhysicalDeviceVulkan11Features vk11Features = {};
        vk11Features.sType                = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        vk11Features.pNext                = &swapchainMaintenanceFeatures;
        vk11Features.shaderDrawParameters = VK_TRUE;
        vk11Features.multiview            = VK_TRUE;

        // Add required Vulkan 1.2 features here
        VkPhysicalDeviceVulkan12Features vk12Features = {};
        vk12Features.sType                                        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        vk12Features.pNext                                        = &vk11Features;
        vk12Features.bufferDeviceAddress                          = VK_TRUE;
        vk12Features.scalarBlockLayout                            = VK_TRUE;
        vk12Features.descriptorIndexing                           = VK_TRUE;
        vk12Features.shaderSampledImageArrayNonUniformIndexing    = VK_TRUE;
        vk12Features.runtimeDescriptorArray                       = VK_TRUE;
        vk12Features.descriptorBindingVariableDescriptorCount     = VK_TRUE;
        vk12Features.descriptorBindingPartiallyBound              = VK_TRUE;
        vk12Features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
        vk12Features.descriptorBindingUpdateUnusedWhilePending    = VK_TRUE;
        vk12Features.drawIndirectCount                            = VK_TRUE;

        // Add required Vulkan 1.3 features here
        VkPhysicalDeviceVulkan13Features vk13Features = {};
        vk13Features.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        vk13Features.pNext            = &vk12Features;
        vk13Features.synchronization2 = VK_TRUE;
        vk13Features.dynamicRendering = VK_TRUE;
        vk13Features.maintenance4     = VK_TRUE;

        // Add required features here
        VkPhysicalDeviceFeatures2 deviceFeatures = {};
        deviceFeatures.sType                         = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures.pNext                         = &vk13Features;
        deviceFeatures.features.samplerAnisotropy    = VK_TRUE;
        deviceFeatures.features.fillModeNonSolid     = VK_TRUE;
        deviceFeatures.features.multiDrawIndirect    = VK_TRUE;
        deviceFeatures.features.textureCompressionBC = VK_TRUE;
        deviceFeatures.features.imageCubeArray       = VK_TRUE;
        deviceFeatures.features.depthClamp           = VK_TRUE;

        const VkDeviceCreateInfo createInfo =
        {
            .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext                   = &deviceFeatures,
            .flags                   = 0,
            .queueCreateInfoCount    = static_cast<u32>(queueCreateInfos.size()),
            .pQueueCreateInfos       = queueCreateInfos.data(),
            #ifdef ENGINE_ENABLE_VALIDATION
            .enabledLayerCount       = static_cast<u32>(VALIDATION_LAYERS.size()),
            .ppEnabledLayerNames     = VALIDATION_LAYERS.data(),
            #else
            .enabledLayerCount       = 0,
            .ppEnabledLayerNames     = nullptr,
            #endif
            .enabledExtensionCount   = static_cast<u32>(REQUIRED_DEVICE_EXTENSIONS.size()),
            .ppEnabledExtensionNames = REQUIRED_DEVICE_EXTENSIONS.data(),
            .pEnabledFeatures        = nullptr
        };

        Vk::CheckResult(vkCreateDevice(
            physicalDevice,
            &createInfo,
            nullptr,
            &device),
            "Failed to create logical device!"
        );

        volkLoadDevice(device);

        Logger::Info("Created logical device! [handle={}]\n", std::bit_cast<void*>(device));

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
            Vk::CheckResult(vkResetCommandPool(device, commandPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT), "Failed to reset command pool!");
            vkDestroyCommandPool(device, commandPool, nullptr);
        });
    }

    void Context::CreateAllocator()
    {
        const VmaVulkanFunctions vulkanFunctions =
        {
            .vkGetInstanceProcAddr                   = vkGetInstanceProcAddr,
            .vkGetDeviceProcAddr                     = vkGetDeviceProcAddr,
            .vkGetPhysicalDeviceProperties           = vkGetPhysicalDeviceProperties,
            .vkGetPhysicalDeviceMemoryProperties     = vkGetPhysicalDeviceMemoryProperties,
            .vkAllocateMemory                        = vkAllocateMemory,
            .vkFreeMemory                            = vkFreeMemory,
            .vkMapMemory                             = vkMapMemory,
            .vkUnmapMemory                           = vkUnmapMemory,
            .vkFlushMappedMemoryRanges               = vkFlushMappedMemoryRanges,
            .vkInvalidateMappedMemoryRanges          = vkInvalidateMappedMemoryRanges,
            .vkBindBufferMemory                      = vkBindBufferMemory,
            .vkBindImageMemory                       = vkBindImageMemory,
            .vkGetBufferMemoryRequirements           = vkGetBufferMemoryRequirements,
            .vkGetImageMemoryRequirements            = vkGetImageMemoryRequirements,
            .vkCreateBuffer                          = vkCreateBuffer,
            .vkDestroyBuffer                         = vkDestroyBuffer,
            .vkCreateImage                           = vkCreateImage,
            .vkDestroyImage                          = vkDestroyImage,
            .vkCmdCopyBuffer                         = vkCmdCopyBuffer,
            .vkGetBufferMemoryRequirements2KHR       = vkGetBufferMemoryRequirements2,
            .vkGetImageMemoryRequirements2KHR        = vkGetImageMemoryRequirements2,
            .vkBindBufferMemory2KHR                  = vkBindBufferMemory2,
            .vkBindImageMemory2KHR                   = vkBindImageMemory2,
            .vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2,
            .vkGetDeviceBufferMemoryRequirements     = vkGetDeviceBufferMemoryRequirements,
            .vkGetDeviceImageMemoryRequirements      = vkGetDeviceImageMemoryRequirements,
            .vkGetMemoryWin32HandleKHR               = nullptr
        };

        const VmaAllocatorCreateInfo createInfo =
        {
            .flags                       = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT   |
                                           VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT |
                                           VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT       |
                                           VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE4_BIT,
            .physicalDevice              = physicalDevice,
            .device                      = device,
            .preferredLargeHeapBlockSize = 0,
            .pAllocationCallbacks        = nullptr,
            .pDeviceMemoryCallbacks      = nullptr,
            .pHeapSizeLimit              = nullptr,
            .pVulkanFunctions            = &vulkanFunctions,
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

    void Context::AddDebugNames()
    {
        Vk::SetDebugName(device, instance,       "Instance");
        Vk::SetDebugName(device, physicalDevice, "PhysicalDevice");
        Vk::SetDebugName(device, device,         "Device");
        Vk::SetDebugName(device, surface,        "SDL3Surface");
        Vk::SetDebugName(device, graphicsQueue,  "GraphicsQueue");
        Vk::SetDebugName(device, commandPool,    "GlobalCommandPool");
    }

    void Context::Destroy()
    {
        m_deletionQueue.FlushQueue();

        vkDestroyDevice(device, nullptr);

        #ifdef ENGINE_ENABLE_VALIDATION
        m_layers.Destroy(instance);
        #endif

        vkDestroyInstance(instance, nullptr);

        volkFinalize();

        Logger::Info("{}\n", "Destroyed vulkan context!");
    }
}