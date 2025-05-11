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

#ifndef DEBUG_UTILS_H
#define DEBUG_UTILS_H

#include <string_view>
#include <vulkan/vulkan.h>
#include <volk/volk.h>

#include "Util.h"
#include "CommandBuffer.h"
#include "Util/Util.h"

namespace Vk
{
    template <typename>
    struct VulkanObjectType;

    template <>
    struct VulkanObjectType<VkBuffer>
    {
        static constexpr VkObjectType ObjectType = VK_OBJECT_TYPE_BUFFER;
    };

    template <>
    struct VulkanObjectType<VkInstance>
    {
        static constexpr VkObjectType ObjectType = VK_OBJECT_TYPE_INSTANCE;
    };

    template <>
    struct VulkanObjectType<VkPhysicalDevice>
    {
        static constexpr VkObjectType ObjectType = VK_OBJECT_TYPE_PHYSICAL_DEVICE;
    };

    template <>
    struct VulkanObjectType<VkDevice>
    {
        static constexpr VkObjectType ObjectType = VK_OBJECT_TYPE_DEVICE;
    };

    template <>
    struct VulkanObjectType<VkSurfaceKHR>
    {
        static constexpr VkObjectType ObjectType = VK_OBJECT_TYPE_SURFACE_KHR;
    };

    template <>
    struct VulkanObjectType<VkQueue>
    {
        static constexpr VkObjectType ObjectType = VK_OBJECT_TYPE_QUEUE;
    };

    template <>
    struct VulkanObjectType<VkCommandPool>
    {
        static constexpr VkObjectType ObjectType = VK_OBJECT_TYPE_COMMAND_POOL;
    };

    template <>
    struct VulkanObjectType<VkCommandBuffer>
    {
        static constexpr VkObjectType ObjectType = VK_OBJECT_TYPE_COMMAND_BUFFER;
    };

    template <>
    struct VulkanObjectType<VkImage>
    {
        static constexpr VkObjectType ObjectType = VK_OBJECT_TYPE_IMAGE;
    };

    template <>
    struct VulkanObjectType<VkImageView>
    {
        static constexpr VkObjectType ObjectType = VK_OBJECT_TYPE_IMAGE_VIEW;
    };

    template <>
    struct VulkanObjectType<VkSampler>
    {
        static constexpr VkObjectType ObjectType = VK_OBJECT_TYPE_SAMPLER;
    };

    template <>
    struct VulkanObjectType<VkPipeline>
    {
        static constexpr VkObjectType ObjectType = VK_OBJECT_TYPE_PIPELINE;
    };

    template <>
    struct VulkanObjectType<VkPipelineLayout>
    {
        static constexpr VkObjectType ObjectType = VK_OBJECT_TYPE_PIPELINE_LAYOUT;
    };

    template <>
    struct VulkanObjectType<VkSemaphore>
    {
        static constexpr VkObjectType ObjectType = VK_OBJECT_TYPE_SEMAPHORE;
    };

    template <>
    struct VulkanObjectType<VkFence>
    {
        static constexpr VkObjectType ObjectType = VK_OBJECT_TYPE_FENCE;
    };

    template <>
    struct VulkanObjectType<VkSwapchainKHR>
    {
        static constexpr VkObjectType ObjectType = VK_OBJECT_TYPE_SWAPCHAIN_KHR;
    };

    template <>
    struct VulkanObjectType<VkDescriptorPool>
    {
        static constexpr VkObjectType ObjectType = VK_OBJECT_TYPE_DESCRIPTOR_POOL;
    };

    template <>
    struct VulkanObjectType<VkDescriptorSetLayout>
    {
        static constexpr VkObjectType ObjectType = VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT;
    };

    template <>
    struct VulkanObjectType<VkDescriptorSet>
    {
        static constexpr VkObjectType ObjectType = VK_OBJECT_TYPE_DESCRIPTOR_SET;
    };

    template <>
    struct VulkanObjectType<VkShaderModule>
    {
        static constexpr VkObjectType ObjectType = VK_OBJECT_TYPE_SHADER_MODULE;
    };

    template <>
    struct VulkanObjectType<VkAccelerationStructureKHR>
    {
        static constexpr VkObjectType ObjectType = VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR;
    };

    template<typename T>
    void SetDebugName(ENGINE_UNUSED VkDevice device, ENGINE_UNUSED T object, ENGINE_UNUSED const std::string_view name)
    {
        #ifdef ENGINE_DEBUG
        if (object == VK_NULL_HANDLE)
        {
            return;
        }

        const VkDebugUtilsObjectNameInfoEXT debugUtilsObjectNameInfo =
        {
            .sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .pNext        = nullptr,
            .objectType   = VulkanObjectType<T>::ObjectType,
            .objectHandle = std::bit_cast<u64>(object),
            .pObjectName  = name.data(),
        };

        Vk::CheckResult(vkSetDebugUtilsObjectNameEXT(
            device,
            &debugUtilsObjectNameInfo),
        "Failed to set object name!");
        #endif
    }

    void BeginLabel(const Vk::CommandBuffer& cmdBuffer, const std::string_view name, const glm::vec4& color);
    void EndLabel(const Vk::CommandBuffer& cmdBuffer);

    void BeginLabel(VkQueue queue, const std::string_view name, const glm::vec4& color);
    void EndLabel(VkQueue queue);
}

#endif
