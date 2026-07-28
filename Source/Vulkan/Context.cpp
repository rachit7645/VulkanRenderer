/*
 * Copyright (c) 2023 - 2026 Rachit
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

#include <vector>
#include <volk/volk.h>

#include "Constants.h"
#include "DebugUtils.h"
#include "Extensions.h"
#include "Util.h"
#include "Util/Containers.h"
#include "Util/Log.h"

namespace Vk
{
    Context::Context(SDL_Window* window, Stack::Allocator& scratchAllocator)
    {
        Vk::CheckResult(volkInitialize(), "Failed to initialize volk!");

        CreateInstance(scratchAllocator);

        CreateSurface(window);

        PickPhysicalDevice(scratchAllocator);
        CreateLogicalDevice(scratchAllocator);

        CreateAllocator();

        AddDebugNames();
    }

    void Context::CreateInstance(Stack::Allocator& scratchAllocator)
    {
        constexpr VkApplicationInfo appInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext              = nullptr,
            .pApplicationName   = Vk::VULKAN_APPLICATION_NAME,
            .applicationVersion = VK_MAKE_API_VERSION(0, 0, 0, 1),
            .pEngineName        = Vk::VULKAN_ENGINE_NAME,
            .engineVersion      = VK_MAKE_API_VERSION(0, 0, 0, 1),
            .apiVersion         = Vk::VULKAN_API_VERSION
        };

        const auto instanceExtensions = Extensions::GetInstanceExtensions(scratchAllocator);

        const VkInstanceCreateInfo createInfo =
        {
            .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            #ifdef ENGINE_DEBUG
            .pNext                   = &m_debugCallback.messengerInfo,
            #else
            .pNext                   = nullptr,
            #endif
            .flags                   = 0,
            .pApplicationInfo        = &appInfo,
            .enabledLayerCount       = 0,
            .ppEnabledLayerNames     = nullptr,
            .enabledExtensionCount   = static_cast<u32>(instanceExtensions.size()),
            .ppEnabledExtensionNames = instanceExtensions.data(),
        };

        Vk::CheckResult(vkCreateInstance(
            &createInfo,
            nullptr,
            &instance),
            "Failed to initialise vulkan instance!"
        );

        volkLoadInstanceOnly(instance);

        #ifdef ENGINE_DEBUG
        m_debugCallback.SetupMessenger(instance);
        #endif
    }

    void Context::CreateSurface(SDL_Window* window)
    {
        if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface))
        {
            Logger::Error
            (
                "Failed to create surface! [window={}] [instance={}]\n",
                reinterpret_cast<void*>(window),
                reinterpret_cast<void*>(instance)
            );
        }

        m_deletionQueue.Push([this] ()
        {
            SDL_Vulkan_DestroySurface(instance, surface, nullptr);
        });
    }

    void Context::PickPhysicalDevice(Stack::Allocator& scratchAllocator)
    {
        u32 deviceCount = 0;

        Vk::CheckResult(vkEnumeratePhysicalDevices(
            instance,
            &deviceCount,
            nullptr),
            "Failed to get physical device count!"
        );

        if (deviceCount == 0)
        {
            Logger::Error("No physical devices found! [instance={}]\n", reinterpret_cast<void*>(instance));
        }

        auto devices = Stack::CreateVector<VkPhysicalDevice>(scratchAllocator);

        devices.resize(deviceCount);

        Vk::CheckResult(vkEnumeratePhysicalDevices(
            instance,
            &deviceCount,
            devices.data()),
            "Failed to get physical devices!"
        );

        auto vkProperties         = Stack::CreateMap<VkPhysicalDevice, VkPhysicalDeviceProperties2>(scratchAllocator);
        auto vk11Properties       = Stack::CreateMap<VkPhysicalDevice, VkPhysicalDeviceVulkan11Properties>(scratchAllocator);
        auto vk12Properties       = Stack::CreateMap<VkPhysicalDevice, VkPhysicalDeviceVulkan12Properties>(scratchAllocator);
        auto asProperties         = Stack::CreateMap<VkPhysicalDevice, VkPhysicalDeviceAccelerationStructurePropertiesKHR>(scratchAllocator);
        auto rtPipelineProperties = Stack::CreateMap<VkPhysicalDevice, VkPhysicalDeviceRayTracingPipelinePropertiesKHR>(scratchAllocator);

        auto features = Stack::CreateMap<VkPhysicalDevice, VkPhysicalDeviceFeatures2>(scratchAllocator);
        auto scores   = Stack::CreateMap<VkPhysicalDevice, usize>(scratchAllocator);

        vkProperties.reserve(deviceCount);
        vk11Properties.reserve(deviceCount);
        vk12Properties.reserve(deviceCount);
        asProperties.reserve(deviceCount);
        rtPipelineProperties.reserve(deviceCount);

        features.reserve(deviceCount);
        scores.reserve(deviceCount);

        for (const auto& currentDevice : devices)
        {
            VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtPipelinePropertySet = {};
            rtPipelinePropertySet.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
            rtPipelinePropertySet.pNext = nullptr;

            VkPhysicalDeviceAccelerationStructurePropertiesKHR asPropertySet = {};
            asPropertySet.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
            asPropertySet.pNext = &rtPipelinePropertySet;

            VkPhysicalDeviceVulkan12Properties vk12PropertySet = {}; // NOLINT(bugprone-invalid-enum-default-initialization)
            vk12PropertySet.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES;
            vk12PropertySet.pNext = &asPropertySet;

            VkPhysicalDeviceVulkan11Properties vk11PropertySet = {};
            vk11PropertySet.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES;
            vk11PropertySet.pNext = &vk12PropertySet;

            VkPhysicalDeviceProperties2 propertySet = {};
            propertySet.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            propertySet.pNext = &vk11PropertySet;

            #ifdef ENGINE_DEBUG
            VkPhysicalDeviceShaderRelaxedExtendedInstructionFeaturesKHR shaderRelaxedExtendedInstructionFeatures = {};
            shaderRelaxedExtendedInstructionFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_RELAXED_EXTENDED_INSTRUCTION_FEATURES_KHR;
            shaderRelaxedExtendedInstructionFeatures.pNext = nullptr;
            #endif

            VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT swapchainMaintenanceFeatures = {};
            swapchainMaintenanceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT;
            #ifdef ENGINE_DEBUG
            swapchainMaintenanceFeatures.pNext = &shaderRelaxedExtendedInstructionFeatures;
            #else
            swapchainMaintenanceFeatures.pNext = nullptr;
            #endif

            VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR rayTracingMaintenance1Features = {};
            rayTracingMaintenance1Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_MAINTENANCE_1_FEATURES_KHR;
            rayTracingMaintenance1Features.pNext = &swapchainMaintenanceFeatures;

            VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures = {};
            rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
            rayTracingPipelineFeatures.pNext = &rayTracingMaintenance1Features;

            VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = {};
            accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
            accelerationStructureFeatures.pNext = &rayTracingPipelineFeatures;

            VkPhysicalDeviceVulkan11Features vk11Features = {};
            vk11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
            vk11Features.pNext = &accelerationStructureFeatures;

            VkPhysicalDeviceVulkan12Features vk12Features = {};
            vk12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
            vk12Features.pNext = &vk11Features;

            VkPhysicalDeviceVulkan13Features vk13Features = {};
            vk13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
            vk13Features.pNext = &vk12Features;

            VkPhysicalDeviceVulkan14Features vk14Features = {};
            vk14Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
            vk14Features.pNext = &vk13Features;

            VkPhysicalDeviceFeatures2 featureSet = {};
            featureSet.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            featureSet.pNext = &vk14Features;

            vkGetPhysicalDeviceProperties2(currentDevice, &propertySet);
            vkGetPhysicalDeviceFeatures2(currentDevice, &featureSet);

            vkProperties.emplace(currentDevice, propertySet);
            vk11Properties.emplace(currentDevice, vk11PropertySet);
            vk12Properties.emplace(currentDevice, vk12PropertySet);
            asProperties.emplace(currentDevice, asPropertySet);
            rtPipelineProperties.emplace(currentDevice, rtPipelinePropertySet);

            features.emplace(currentDevice, featureSet);

            const usize score = Vk::CalculatePhysicalDeviceScore
            (
                instance,
                currentDevice,
                surface,
                propertySet,
                featureSet,
                scratchAllocator
            );

            scores.emplace(currentDevice, score);
        }

        VkPhysicalDevice bestDevice   = VK_NULL_HANDLE;
        usize            highestScore = 0;

        for (const auto& [currentDevice, score] : scores)
        {
            if (score > highestScore)
            {
                highestScore = score;
                bestDevice   = currentDevice;
            }
        }

        // Score = 0 => Required features not supported
        if (highestScore == 0)
        {
            Logger::Error("Failed to find any suitable physical device!");
        }

        physicalDevice     = bestDevice;
        physicalDeviceName = vkProperties[physicalDevice].properties.deviceName;

        properties = Vk::Properties
        (
            vkProperties[physicalDevice].properties.limits,
            vk12Properties[physicalDevice],
            asProperties[physicalDevice],
            rtPipelineProperties[physicalDevice]
        );

        Logger::Info("Selected GPU! [GPU={}]\n", physicalDeviceName);
    }

    void Context::CreateLogicalDevice(Stack::Allocator& scratchAllocator)
    {
        queueFamilies = Vk::QueueFamilies(physicalDevice, surface, scratchAllocator);
        extensions    = Vk::Extensions(physicalDevice);

        const auto uniqueQueueFamilies = queueFamilies.GetUniqueFamilies(scratchAllocator);

        auto queueCreateInfos = Stack::CreateVector<VkDeviceQueueCreateInfo>(scratchAllocator);

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

        const auto deviceExtensions = extensions.GetDeviceExtensions
        (
            instance,
            physicalDevice,
            scratchAllocator
        );

        VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR rayTracingMaintenance1Features = {};
        rayTracingMaintenance1Features.sType                  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_MAINTENANCE_1_FEATURES_KHR;
        rayTracingMaintenance1Features.pNext                  = nullptr;
        rayTracingMaintenance1Features.rayTracingMaintenance1 = VK_TRUE;

        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures = {};
        rayTracingPipelineFeatures.sType              = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
        rayTracingPipelineFeatures.pNext              = &rayTracingMaintenance1Features;
        rayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;

        VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = {};
        accelerationStructureFeatures.sType                 = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        accelerationStructureFeatures.pNext                 = &rayTracingPipelineFeatures;
        accelerationStructureFeatures.accelerationStructure = VK_TRUE;

        #ifdef ENGINE_DEBUG
        VkPhysicalDeviceShaderRelaxedExtendedInstructionFeaturesKHR shaderRelaxedExtendedInstructionFeatures = {};
        shaderRelaxedExtendedInstructionFeatures.sType                            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_RELAXED_EXTENDED_INSTRUCTION_FEATURES_KHR;
        shaderRelaxedExtendedInstructionFeatures.shaderRelaxedExtendedInstruction = VK_TRUE;
        #endif

        VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT swapchainMaintenanceFeatures = {};
        swapchainMaintenanceFeatures.sType                 = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT;
        swapchainMaintenanceFeatures.swapchainMaintenance1 = VK_TRUE;

        #ifdef ENGINE_DEBUG
        swapchainMaintenanceFeatures.pNext             = &shaderRelaxedExtendedInstructionFeatures;
        shaderRelaxedExtendedInstructionFeatures.pNext = &accelerationStructureFeatures;
        #else
        swapchainMaintenanceFeatures.pNext = &accelerationStructureFeatures;
        #endif

        VkPhysicalDeviceVulkan11Features vk11Features = {};
        vk11Features.sType                    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        vk11Features.pNext                    = &swapchainMaintenanceFeatures;
        vk11Features.shaderDrawParameters     = VK_TRUE;
        vk11Features.multiview                = VK_TRUE;
        vk11Features.storageBuffer16BitAccess = VK_TRUE;

        VkPhysicalDeviceVulkan12Features vk12Features = {};
        vk12Features.sType                                        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        vk12Features.pNext                                        = &vk11Features;
        vk12Features.bufferDeviceAddress                          = VK_TRUE;
        vk12Features.scalarBlockLayout                            = VK_TRUE;
        vk12Features.descriptorIndexing                           = VK_TRUE;
        vk12Features.shaderSampledImageArrayNonUniformIndexing    = VK_TRUE;
        vk12Features.shaderStorageImageArrayNonUniformIndexing    = VK_TRUE;
        vk12Features.runtimeDescriptorArray                       = VK_TRUE;
        vk12Features.descriptorBindingVariableDescriptorCount     = VK_TRUE;
        vk12Features.descriptorBindingPartiallyBound              = VK_TRUE;
        vk12Features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
        vk12Features.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
        vk12Features.descriptorBindingUpdateUnusedWhilePending    = VK_TRUE;
        vk12Features.drawIndirectCount                            = VK_TRUE;
        vk12Features.timelineSemaphore                            = VK_TRUE;
        vk12Features.shaderFloat16                                = VK_TRUE;

        VkPhysicalDeviceVulkan13Features vk13Features = {};
        vk13Features.sType                          = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        vk13Features.pNext                          = &vk12Features;
        vk13Features.synchronization2               = VK_TRUE;
        vk13Features.dynamicRendering               = VK_TRUE;
        vk13Features.maintenance4                   = VK_TRUE;
        vk13Features.shaderDemoteToHelperInvocation = VK_TRUE;

        VkPhysicalDeviceVulkan14Features vk14Features = {};
        vk14Features.sType          = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
        vk14Features.pNext          = &vk13Features;
        vk14Features.maintenance5   = VK_TRUE;
        #ifdef ENGINE_DLSS
        vk14Features.pushDescriptor = std::ranges::contains(deviceExtensions, Util::ToLower(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME), Util::ToLower); // Fuck you Jensen Huang
        #endif

        VkPhysicalDeviceFeatures2 deviceFeatures = {};
        deviceFeatures.sType                         = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures.pNext                         = &vk14Features;
        deviceFeatures.features.samplerAnisotropy    = VK_TRUE;
        deviceFeatures.features.multiDrawIndirect    = VK_TRUE;
        deviceFeatures.features.textureCompressionBC = VK_TRUE;
        deviceFeatures.features.imageCubeArray       = VK_TRUE;
        deviceFeatures.features.depthClamp           = VK_TRUE;
        deviceFeatures.features.shaderInt64          = VK_TRUE;
        deviceFeatures.features.fullDrawIndexUint32  = VK_TRUE;
        deviceFeatures.features.shaderInt16          = VK_TRUE;

        const VkDeviceCreateInfo createInfo =
        {
            .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext                   = &deviceFeatures,
            .flags                   = 0,
            .queueCreateInfoCount    = static_cast<u32>(queueCreateInfos.size()),
            .pQueueCreateInfos       = queueCreateInfos.data(),
            .enabledLayerCount       = 0,
            .ppEnabledLayerNames     = nullptr,
            .enabledExtensionCount   = static_cast<u32>(deviceExtensions.size()),
            .ppEnabledExtensionNames = deviceExtensions.data(),
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

        const VkDeviceQueueInfo2 graphicsQueueInfo =
        {
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2,
            .pNext            = nullptr,
            .flags            = 0,
            .queueFamilyIndex = *queueFamilies.graphicsFamily,
            .queueIndex       = 0
        };

        vkGetDeviceQueue2
        (
            device,
            &graphicsQueueInfo,
            &graphicsQueue
        );

        if (queueFamilies.computeFamily.has_value())
        {
            const VkDeviceQueueInfo2 computeQueueInfo =
            {
                .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2,
                .pNext            = nullptr,
                .flags            = 0,
                .queueFamilyIndex = *queueFamilies.computeFamily,
                .queueIndex       = 0
            };

            vkGetDeviceQueue2
            (
                device,
                &computeQueueInfo,
                &computeQueue
            );
        }
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
            .vkGetMemoryWin32HandleKHR               = nullptr,
            .vkGetPhysicalDeviceProperties2KHR       = vkGetPhysicalDeviceProperties2
        };

        VmaAllocatorCreateFlags flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT |
                                        VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE4_BIT |
                                        VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE5_BIT;

        if (extensions.HasMemoryBudget())
        {
            flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
        }

        const VmaAllocatorCreateInfo createInfo =
        {
            .flags                       = flags,
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

        m_deletionQueue.Push([this] ()
        {
            vmaDestroyAllocator(allocator);
        });
    }

    void Context::AddDebugNames() const
    {
        Vk::SetDebugName(device, instance,       "Instance");
        Vk::SetDebugName(device, physicalDevice, "PhysicalDevice");
        Vk::SetDebugName(device, device,         "Device");
        Vk::SetDebugName(device, surface,        "SDL3/Surface");
        Vk::SetDebugName(device, graphicsQueue,  "Queue/Graphics");

        if (queueFamilies.computeFamily.has_value())
        {
            Vk::SetDebugName(device, computeQueue, "Queue/Compute");
        }
    }

    void Context::Destroy()
    {
        m_deletionQueue.Flush();

        vkDestroyDevice(device, nullptr);

        #ifdef ENGINE_DEBUG
        m_debugCallback.Destroy(instance);
        #endif

        vkDestroyInstance(instance, nullptr);

        volkFinalize();
    }
}