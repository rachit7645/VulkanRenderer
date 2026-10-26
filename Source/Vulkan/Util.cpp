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

#include <vulkan/utility/vk_format_utils.h>
#include <vulkan/vk_enum_string_helper.h>

#include "Util.h"
#include "DebugUtils.h"
#include "Chain.h"
#include "Extensions.h"
#include "QueueFamilies.h"
#include "SwapchainInfo.h"
#include "Externals/DLSSNoSDK.h"
#include "Util/Log.h"

namespace Vk
{
    usize CalculatePhysicalDeviceScore
    (
        ENGINE_UNUSED VkInstance instance,
        VkPhysicalDevice physicalDevice,
        VkSurfaceKHR surface,
        const VkPhysicalDeviceProperties2& properties,
        const VkPhysicalDeviceFeatures2& features,
        Scratch::Allocator& scratchAllocator
    )
    {
        const auto queues            = Vk::QueueFamilies(physicalDevice, surface, scratchAllocator);
        const auto currentExtensions = Vk::Extensions(physicalDevice);

        const auto* vk11Properties = Vk::FindStructureInChain<VkPhysicalDeviceVulkan11Properties>(properties.pNext);

        const auto* vk11Features = Vk::FindStructureInChain<VkPhysicalDeviceVulkan11Features>(features.pNext);
        const auto* vk12Features = Vk::FindStructureInChain<VkPhysicalDeviceVulkan12Features>(features.pNext);
        const auto* vk13Features = Vk::FindStructureInChain<VkPhysicalDeviceVulkan13Features>(features.pNext);
        const auto* vk14Features = Vk::FindStructureInChain<VkPhysicalDeviceVulkan14Features>(features.pNext);

        const auto* swapchainMaintenanceFeatures = Vk::FindStructureInChain<VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT>(features.pNext);

        const auto* accelerationStructureFeatures = Vk::FindStructureInChain<VkPhysicalDeviceAccelerationStructureFeaturesKHR>(features.pNext);
        const auto* rayTracingFeatures            = Vk::FindStructureInChain<VkPhysicalDeviceRayTracingPipelineFeaturesKHR>(features.pNext);
        const auto* rayTracingMaintenance1        = Vk::FindStructureInChain<VkPhysicalDeviceRayTracingMaintenance1FeaturesKHR>(features.pNext);

        #ifdef ENGINE_DEBUG
        const auto* shaderRelaxedExtendedInstructionFeatures = Vk::FindStructureInChain<VkPhysicalDeviceShaderRelaxedExtendedInstructionFeaturesKHR>(features.pNext);
        #endif

        // Score parts
        const usize discreteGPU    = (properties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) ? 10000 : 100;
        const usize completeQueues = queues.HasAllFamilies() ? 1000 : 0;

        // Requirements
        const bool hasRequiredQueueFamilies = queues.HasRequiredFamilies();
        const bool hasRequiredExtensions    = currentExtensions.HasRequiredExtensions();

        #ifdef ENGINE_DLSS
        const bool arePushDescriptorsRequired = std::ranges::contains
        (
            DLSS::GetDeviceExtensions(instance, physicalDevice, scratchAllocator),
            Util::ToLower(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME),
            Util::ToLower
        );
        #else
        constexpr bool arePushDescriptorsRequired = false;
        #endif

        // Need extensions to calculate these
        bool isSwapChainAdequate     = false;
        bool hasSwapchainMaintenance = false;
        bool hasRayTracing           = false;

        #ifdef ENGINE_DEBUG
        bool hasShaderRelaxedExtendedInstruction = false;
        #endif

        if (hasRequiredExtensions)
        {
            const auto swapChainInfo = Vk::SwapchainInfo(physicalDevice, surface);

            isSwapChainAdequate     = !(swapChainInfo.formats.empty() || swapChainInfo.presentModes.empty());
            hasSwapchainMaintenance = swapchainMaintenanceFeatures->swapchainMaintenance1;

            #ifdef ENGINE_DEBUG
            hasShaderRelaxedExtendedInstruction = shaderRelaxedExtendedInstructionFeatures->shaderRelaxedExtendedInstruction;
            #endif

            const bool hasAccelerationStructure = accelerationStructureFeatures->accelerationStructure;
            const bool hasRayTracingPipeline    = rayTracingFeatures->rayTracingPipeline;
            const bool hasRayTracingMaintenance = rayTracingMaintenance1->rayTracingMaintenance1;

            hasRayTracing = hasAccelerationStructure && hasRayTracingPipeline && hasRayTracingMaintenance;
        }

        // Standard features
        const bool hasPushConstantSize  = properties.properties.limits.maxPushConstantsSize >= 128;
        const bool hasAnisotropy        = features.features.samplerAnisotropy;
        const bool hasMultiDrawIndirect = features.features.multiDrawIndirect;
        const bool hasBC                = features.features.textureCompressionBC;
        const bool hasImageCubeArray    = features.features.imageCubeArray;
        const bool hasDepthClamp        = features.features.depthClamp;
        const bool hasInt64             = features.features.shaderInt64;
        const bool indexU32             = features.features.fullDrawIndexUint32;
        const bool hasInt16             = features.features.shaderInt16;

        // Vulkan 1.1 features
        const bool hasRequiredMultiViewCount      = vk11Properties->maxMultiviewViewCount >= 6;
        const bool hasShaderDrawParameters        = vk11Features->shaderDrawParameters;
        const bool hasMultiView                   = vk11Features->multiview;
        const bool hasSubgroupOperationsInCompute = vk11Properties->subgroupSupportedStages & VK_SHADER_STAGE_COMPUTE_BIT;
        const bool hasSubgroupBasic               = vk11Properties->subgroupSupportedOperations & VK_SUBGROUP_FEATURE_BASIC_BIT;
        const bool hasSubgroupArithmetic          = vk11Properties->subgroupSupportedOperations & VK_SUBGROUP_FEATURE_ARITHMETIC_BIT;
        const bool hasStorageF16                  = vk11Features->storageBuffer16BitAccess;

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
        const bool hasShaderF16                      = vk12Features->shaderFloat16;

        // Vulkan 1.3 features
        const bool hasSync2          = vk13Features->synchronization2;
        const bool hasDynRender      = vk13Features->dynamicRendering;
        const bool hasMaintenance4   = vk13Features->maintenance4;
        const bool hasDemoteToHelper = vk13Features->shaderDemoteToHelperInvocation;

        // Vulkan 1.4 features
        const bool hasMaintenance5   = vk14Features->maintenance5;
        const bool hasPushDescriptor = vk14Features->pushDescriptor || !arePushDescriptorsRequired;

        const bool hasRequired = hasRequiredQueueFamilies && hasRequiredExtensions;

        const bool hasStandard = hasPushConstantSize && hasAnisotropy && hasMultiDrawIndirect && hasBC &&
                                 hasImageCubeArray && hasDepthClamp && hasInt64 && indexU32 && hasInt16;

        const bool hasExtensions = isSwapChainAdequate && hasSwapchainMaintenance && hasRayTracing
                                   #ifdef ENGINE_DEBUG
                                   && hasShaderRelaxedExtendedInstruction
                                   #endif
                                   ;

        const bool hasVk11 = hasRequiredMultiViewCount && hasShaderDrawParameters && hasMultiView &&
                             hasSubgroupOperationsInCompute && hasSubgroupBasic && hasSubgroupArithmetic && hasStorageF16;

        const bool hasVk12 = hasBDA && hasScalarLayout && hasDescriptorIndexing && hasSampledImageNonUniformIndexing &&
                             hasStorageImageNonUniformIndexing && hasRuntimeDescriptorArray && hasPartiallyBoundDescriptors &&
                             hasSampledImageUpdateAfterBind && hasStorageImageUpdateAfterBind && hasUpdateUnusedWhilePending &&
                             hasDrawIndirectCount && hasTimelineSemaphore && hasShaderF16;

        const bool hasVk13 = hasSync2 && hasDynRender && hasMaintenance4 && hasDemoteToHelper;

        const bool hasVk14 = hasMaintenance5 && hasPushDescriptor;

        const usize totalScore = discreteGPU + completeQueues;

        return (hasRequired && hasStandard && hasExtensions && hasVk11 && hasVk12 && hasVk13 && hasVk14) * totalScore;
    }

    VkFormat FindSupportedFormat
    (
        VkPhysicalDevice physicalDevice,
        const std::span<const VkFormat> candidates,
        VkImageTiling tiling,
        VkFormatFeatureFlags2 features
    )
    {
        for (const auto format : candidates)
        {
            VkFormatProperties3 properties3 = {};
            properties3.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3;
            properties3.pNext = nullptr;

            VkFormatProperties2 properties2 = {};
            properties2.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
            properties2.pNext = &properties3;

            vkGetPhysicalDeviceFormatProperties2(physicalDevice, format, &properties2);

            const bool isValidLinear  = (tiling == VK_IMAGE_TILING_LINEAR)  && ((properties3.linearTilingFeatures  & features) == features);
            const bool isValidOptimal = (tiling == VK_IMAGE_TILING_OPTIMAL) && ((properties3.optimalTilingFeatures & features) == features);

            if (isValidLinear || isValidOptimal)
            {
                return format;
            }
        }

        Logger::Error
        (
            "No valid formats found! [PhysicalDevice={}] [Tiling={}] [Features={}]\n",
            std::bit_cast<void*>(physicalDevice),
            string_VkImageTiling(tiling),
            string_VkFormatFeatureFlags(features)
        );
    }

    f64 GetTexelSize(VkFormat format)
    {
        const u32        texelBlockSize = vkuFormatTexelBlockSize(format);
        const VkExtent3D blockExtent    = vkuFormatTexelBlockExtent(format);

        const usize texelsPerBlock = static_cast<usize>(blockExtent.width) * static_cast<usize>(blockExtent.height) * static_cast<usize>(blockExtent.depth);

        return texelBlockSize / static_cast<f64>(std::max(texelsPerBlock, 1ull));
    }

    VkDeviceSize GetImageSize(VkFormat format, u32 width, u32 height)
    {
        const usize      texelBlockSize = vkuFormatTexelBlockSize(format);
        const VkExtent3D blockExtent    = vkuFormatTexelBlockExtent(format);

        const usize blockCountX = (static_cast<usize>(width)  + blockExtent.width  - 1) / std::max<usize>(blockExtent.width,  1);
        const usize blockCountY = (static_cast<usize>(height) + blockExtent.height - 1) / std::max<usize>(blockExtent.height, 1);

        return blockCountX * blockCountY * texelBlockSize;
    }

    void CheckResult(VkResult result, const std::string_view message)
    {
        if (result != VK_SUCCESS)
        {
            Logger::VulkanError("[{}] {}\n", string_VkResult(result), message.data());
        }
    }
}