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

#include <vector>
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
    Context::Context(SDL_Window* window)
    {
        Vk::CheckResult(volkInitialize(), "Failed to initialize volk!");

        CreateInstance();

        CreateSurface(window);

        PickPhysicalDevice();
        CreateLogicalDevice();

        CreateAllocator();

        AddDebugNames();
    }

    void Context::CreateInstance()
    {
        constexpr VkApplicationInfo appInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext              = nullptr,
            .pApplicationName   = "Rachit's Vulkan Renderer",
            .applicationVersion = VK_MAKE_API_VERSION(0, 0, 0, 1),
            .pEngineName        = "Rachit's Engine: Vulkan Edition",
            .engineVersion      = VK_MAKE_API_VERSION(0, 0, 0, 1),
            .apiVersion         = VULKAN_API_VERSION
        };

        const auto instanceExtensions = Vk::Extensions::GetInstanceExtensions();

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
                std::bit_cast<void*>(window),
                std::bit_cast<void*>(instance)
            );
        }

        m_deletionQueue.PushDeletor([this] ()
        {
            SDL_Vulkan_DestroySurface(instance, surface, nullptr);
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

        auto properties           = ankerl::unordered_dense::map<VkPhysicalDevice, VkPhysicalDeviceProperties2>(deviceCount);
        auto vk11Properties       = ankerl::unordered_dense::map<VkPhysicalDevice, VkPhysicalDeviceVulkan11Properties>(deviceCount);
        auto vk12Properties       = ankerl::unordered_dense::map<VkPhysicalDevice, VkPhysicalDeviceVulkan12Properties>(deviceCount);
        auto asProperties         = ankerl::unordered_dense::map<VkPhysicalDevice, VkPhysicalDeviceAccelerationStructurePropertiesKHR>(deviceCount);
        auto rtPipelineProperties = ankerl::unordered_dense::map<VkPhysicalDevice, VkPhysicalDeviceRayTracingPipelinePropertiesKHR>(deviceCount);

        auto features = ankerl::unordered_dense::map<VkPhysicalDevice, VkPhysicalDeviceFeatures2>(deviceCount);
        auto scores   = ankerl::unordered_dense::map<VkPhysicalDevice, usize>{};

        for (const auto& currentDevice : devices)
        {
            VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtPipelinePropertySet = {};
            rtPipelinePropertySet.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
            rtPipelinePropertySet.pNext = nullptr;

            VkPhysicalDeviceAccelerationStructurePropertiesKHR asPropertySet = {};
            asPropertySet.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
            asPropertySet.pNext = &rtPipelinePropertySet;

            VkPhysicalDeviceVulkan12Properties vk12PropertySet = {};
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
            vk11Properties.emplace(currentDevice, vk11PropertySet);
            vk12Properties.emplace(currentDevice, vk12PropertySet);
            asProperties.emplace(currentDevice, asPropertySet);
            rtPipelineProperties.emplace(currentDevice, rtPipelinePropertySet);

            features.emplace(currentDevice, featureSet);
            scores.emplace(currentDevice, CalculateScore(currentDevice, propertySet, featureSet));
        }

        VkPhysicalDevice bestDevice   = VK_NULL_HANDLE;
        usize            highestScore = 0;

        for (const auto& [device, score] : scores)
        {
            if (score > highestScore)
            {
                highestScore = score;
                bestDevice   = device;
            }
        }

        // Score = 0 => Required features not supported
        if (highestScore == 0)
        {
            Logger::Error("Failed to find any suitable physical device!");
        }

        physicalDevice                                = bestDevice;
        physicalDeviceLimits                          = properties[physicalDevice].properties.limits;
        physicalDeviceVulkan12Properties              = vk12Properties[physicalDevice];
        physicalDeviceAccelerationStructureProperties = asProperties[physicalDevice];
        physicalDeviceRayTracingPipelineProperties    = rtPipelineProperties[physicalDevice];

        Logger::Info("Selected GPU! [GPU={}]\n", properties[physicalDevice].properties.deviceName);
    }

    usize Context::CalculateScore
    (
        VkPhysicalDevice phyDevice,
        const VkPhysicalDeviceProperties2& propertySet,
        const VkPhysicalDeviceFeatures2& featureSet
    ) const
    {
        const auto queues     = QueueFamilies(phyDevice, surface);
        const auto extensions = Extensions(phyDevice);

        const auto vk11Properties = Vk::FindStructureInChain<VkPhysicalDeviceVulkan11Properties>(propertySet.pNext);

        const auto vk11Features                  = Vk::FindStructureInChain<VkPhysicalDeviceVulkan11Features>(featureSet.pNext);
        const auto vk12Features                  = Vk::FindStructureInChain<VkPhysicalDeviceVulkan12Features>(featureSet.pNext);
        const auto vk13Features                  = Vk::FindStructureInChain<VkPhysicalDeviceVulkan13Features>(featureSet.pNext);
        const auto swapchainMaintenanceFeatures  = Vk::FindStructureInChain<VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT>(featureSet.pNext);

        #ifdef ENGINE_DEBUG
        const auto shaderRelaxedExtendedInstructionFeatures = Vk::FindStructureInChain<VkPhysicalDeviceShaderRelaxedExtendedInstructionFeaturesKHR>(featureSet.pNext);
        #endif

        // Score parts
        const usize discreteGPU       = (propertySet.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) ? 10000 : 100;
        const usize completeQueues    = queues.HasAllFamilies() ? 1000 : 0;
        const usize rayTracingSupport = extensions.HasRayTracing() ? 5000 : 0;

        // Requirements
        const bool hasRequiredQueueFamilies = queues.HasRequiredFamilies();
        const bool hasRequiredExtensions    = extensions.HasRequiredExtensions();

        // Need extensions to calculate these
        bool isSwapChainAdequate     = false;
        bool hasSwapchainMaintenance = false;

        #ifdef ENGINE_DEBUG
        bool hasShaderRelaxedExtendedInstruction = false;
        #endif

        if (hasRequiredExtensions)
        {
            const auto swapChainInfo = Vk::SwapchainInfo(phyDevice, surface);

            isSwapChainAdequate     = !(swapChainInfo.formats.empty() || swapChainInfo.presentModes.empty());
            hasSwapchainMaintenance = swapchainMaintenanceFeatures->swapchainMaintenance1;

            #ifdef ENGINE_DEBUG
            hasShaderRelaxedExtendedInstruction = shaderRelaxedExtendedInstructionFeatures->shaderRelaxedExtendedInstruction;
            #endif
        }

        // Standard features
        const bool hasPushConstantSize  = propertySet.properties.limits.maxPushConstantsSize >= 128;
        const bool hasAnisotropy        = featureSet.features.samplerAnisotropy;
        const bool hasMultiDrawIndirect = featureSet.features.multiDrawIndirect;
        const bool hasBC                = featureSet.features.textureCompressionBC;
        const bool hasImageCubeArray    = featureSet.features.imageCubeArray;
        const bool hasDepthClamp        = featureSet.features.depthClamp;
        const bool hasInt64             = featureSet.features.shaderInt64;
        const bool indexU32             = featureSet.features.fullDrawIndexUint32;

        // Vulkan 1.1 features
        const bool hasRequiredMultiViewCount = vk11Properties->maxMultiviewViewCount >= 6;
        const bool hasShaderDrawParameters   = vk11Features->shaderDrawParameters;
        const bool hasMultiView              = vk11Features->multiview;

        // Vulkan 1.2 features
        const bool hasBDA                            = vk12Features->bufferDeviceAddress;
        const bool hasScalarLayout                   = vk12Features->scalarBlockLayout;
        const bool hasDescriptorIndexing             = vk12Features->descriptorIndexing;
        const bool hasSampledImageNonUniformIndexing = vk12Features->shaderSampledImageArrayNonUniformIndexing;
        const bool hasStorageImageNonUniformIndexing = vk12Features->shaderStorageImageArrayNonUniformIndexing;
        const bool hasRuntimeDescriptorArray         = vk12Features->runtimeDescriptorArray;
        const bool hasPartiallyBoundDescriptors      = vk12Features->descriptorBindingPartiallyBound;
        const bool hasSampledImageUpdateAfterBind    = vk12Features->descriptorBindingSampledImageUpdateAfterBind;
        const bool hasStorageImageUpdateAfterBind    = vk12Features->descriptorBindingStorageImageUpdateAfterBind;
        const bool hasUpdateUnusedWhilePending       = vk12Features->descriptorBindingUpdateUnusedWhilePending;
        const bool hasDrawIndirectCount              = vk12Features->drawIndirectCount;
        const bool hasTimelineSemaphore              = vk12Features->timelineSemaphore;

        // Vulkan 1.3 features
        const bool hasSync2        = vk13Features->synchronization2;
        const bool hasDynRender    = vk13Features->dynamicRendering;
        const bool hasMaintenance4 = vk13Features->maintenance4;

        const bool hasRequired = hasRequiredQueueFamilies && hasRequiredExtensions;

        const bool hasStandard = hasPushConstantSize && hasAnisotropy && hasMultiDrawIndirect && hasBC &&
                                 hasImageCubeArray && hasDepthClamp && hasInt64 && indexU32;

        const bool hasExtensions = isSwapChainAdequate && hasSwapchainMaintenance
                                   #ifdef ENGINE_DEBUG
                                   && hasShaderRelaxedExtendedInstruction
                                   #endif
                                   ;

        const bool hasVK11 = hasRequiredMultiViewCount && hasShaderDrawParameters && hasMultiView;

        const bool hasVK12 = hasBDA && hasScalarLayout && hasDescriptorIndexing && hasSampledImageNonUniformIndexing &&
                             hasStorageImageNonUniformIndexing && hasRuntimeDescriptorArray && hasPartiallyBoundDescriptors &&
                             hasSampledImageUpdateAfterBind && hasStorageImageUpdateAfterBind && hasUpdateUnusedWhilePending &&
                             hasDrawIndirectCount && hasTimelineSemaphore;

        const bool hasVK13 = hasSync2 && hasDynRender && hasMaintenance4;

        const usize totalScore = discreteGPU + completeQueues + rayTracingSupport;

        return (hasRequired && hasStandard && hasExtensions && hasVK11 && hasVK12 && hasVK13) * totalScore;
    }

    void Context::CreateLogicalDevice()
    {
        queueFamilies = QueueFamilies(physicalDevice, surface);
        extensions    = Extensions(physicalDevice);

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

        if (extensions.HasRayTracing())
        {
            #ifdef ENGINE_DEBUG
            swapchainMaintenanceFeatures.pNext             = &shaderRelaxedExtendedInstructionFeatures;
            shaderRelaxedExtendedInstructionFeatures.pNext = &accelerationStructureFeatures;
            #else
            swapchainMaintenanceFeatures.pNext = &accelerationStructureFeatures;
            #endif
        }
        else
        {
            #ifdef ENGINE_DEBUG
            swapchainMaintenanceFeatures.pNext = &shaderRelaxedExtendedInstructionFeatures;
            #else
            swapchainMaintenanceFeatures.pNext = nullptr;
            #endif
        }

        VkPhysicalDeviceVulkan11Features vk11Features = {};
        vk11Features.sType                = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        vk11Features.pNext                = &swapchainMaintenanceFeatures;
        vk11Features.shaderDrawParameters = VK_TRUE;
        vk11Features.multiview            = VK_TRUE;

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

        VkPhysicalDeviceVulkan13Features vk13Features = {};
        vk13Features.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        vk13Features.pNext            = &vk12Features;
        vk13Features.synchronization2 = VK_TRUE;
        vk13Features.dynamicRendering = VK_TRUE;
        vk13Features.maintenance4     = VK_TRUE;

        VkPhysicalDeviceFeatures2 deviceFeatures = {};
        deviceFeatures.sType                         = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        deviceFeatures.pNext                         = &vk13Features;
        deviceFeatures.features.samplerAnisotropy    = VK_TRUE;
        deviceFeatures.features.multiDrawIndirect    = VK_TRUE;
        deviceFeatures.features.textureCompressionBC = VK_TRUE;
        deviceFeatures.features.imageCubeArray       = VK_TRUE;
        deviceFeatures.features.depthClamp           = VK_TRUE;
        deviceFeatures.features.shaderInt64          = VK_TRUE;
        deviceFeatures.features.fullDrawIndexUint32  = VK_TRUE;

        const auto deviceExtensions = extensions.GetDeviceExtensions();

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
            .vkGetMemoryWin32HandleKHR               = nullptr
        };

        VmaAllocatorCreateFlags flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT | VMA_ALLOCATOR_CREATE_KHR_MAINTENANCE4_BIT;

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

        m_deletionQueue.PushDeletor([this] ()
        {
            vmaDestroyAllocator(allocator);
        });
    }

    void Context::AddDebugNames() const
    {
        Vk::SetDebugName(device, instance,       "Instance");
        Vk::SetDebugName(device, physicalDevice, "PhysicalDevice");
        Vk::SetDebugName(device, device,         "Device");
        Vk::SetDebugName(device, surface,        "SDL3Surface");
        Vk::SetDebugName(device, graphicsQueue,  "GraphicsQueue");

        if (queueFamilies.computeFamily.has_value())
        {
            Vk::SetDebugName(device, computeQueue, "ComputeQueue");
        }
    }

    void Context::Destroy()
    {
        m_deletionQueue.FlushQueue();

        vkDestroyDevice(device, nullptr);

        #ifdef ENGINE_DEBUG
        m_debugCallback.Destroy(instance);
        #endif

        vkDestroyInstance(instance, nullptr);

        volkFinalize();
    }
}