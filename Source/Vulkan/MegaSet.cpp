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

#include "MegaSet.h"

#include <volk/volk.h>

#include "DebugUtils.h"
#include "Util/Log.h"
#include "Util.h"

namespace Vk
{
    constexpr u32 MAX_SAMPLERS       = 1 << 8;
    constexpr u32 MAX_SAMPLED_IMAGES = 1 << 16;

    MegaSet::MegaSet(VkDevice device, const VkPhysicalDeviceLimits& deviceLimits)
    {
        const auto maxSamplers      = std::min(deviceLimits.maxDescriptorSetSamplers,      MAX_SAMPLERS);
        const auto maxSampledImages = std::min(deviceLimits.maxDescriptorSetSampledImages, MAX_SAMPLED_IMAGES);

        const std::array poolSizes =
        {
            VkDescriptorPoolSize
            {
                .type            = VK_DESCRIPTOR_TYPE_SAMPLER,
                .descriptorCount = maxSamplers
            },
            VkDescriptorPoolSize
            {
                .type            = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .descriptorCount = maxSampledImages
            }
        };

        const VkDescriptorPoolCreateInfo poolCreateInfo =
        {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext         = nullptr,
            .flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
            .maxSets       = 1,
            .poolSizeCount = poolSizes.size(),
            .pPoolSizes    = poolSizes.data()
        };

        Vk::CheckResult(vkCreateDescriptorPool(
            device,
            &poolCreateInfo,
            nullptr,
            &m_descriptorPool),
            "Failed to create mega set descriptor pool!"
        );

        constexpr std::array<VkDescriptorBindingFlags, DescriptorBinding::BINDINGS_COUNT> bindingFlags =
        {
            // Samplers
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
            // Sampled image
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        };

        const VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCreateInfo =
        {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            .pNext         = nullptr,
            .bindingCount  = bindingFlags.size(),
            .pBindingFlags = bindingFlags.data()
        };

        const std::array bindings =
        {
            VkDescriptorSetLayoutBinding
            {
                .binding            = DescriptorBinding::SAMPLER_BINDING,
                .descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLER,
                .descriptorCount    = maxSamplers,
                .stageFlags         = VK_SHADER_STAGE_ALL,
                .pImmutableSamplers = nullptr
            },
            VkDescriptorSetLayoutBinding
            {
                .binding            = DescriptorBinding::SAMPLED_IMAGES_BINDING,
                .descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .descriptorCount    = maxSampledImages,
                .stageFlags         = VK_SHADER_STAGE_ALL,
                .pImmutableSamplers = nullptr
            }
        };

        const VkDescriptorSetLayoutCreateInfo createInfo =
        {
            .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext        = &bindingFlagsCreateInfo,
            .flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
            .bindingCount = bindings.size(),
            .pBindings    = bindings.data()
        };

        Vk::CheckResult(vkCreateDescriptorSetLayout(
            device,
            &createInfo,
            nullptr,
            &descriptorSet.layout),
            "Failed to create mega set layout!"
        );

        VkDescriptorSetAllocateInfo allocInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext              = nullptr,
            .descriptorPool     = m_descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts        = &descriptorSet.layout
        };

        Vk::CheckResult
        (
            vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet.handle),
            "Failed to allocate mega set"
        );

        Vk::SetDebugName(device, m_descriptorPool,     "MegaSet/DescriptorPool");
        Vk::SetDebugName(device, descriptorSet.layout, "MegaSet/DescriptorLayout");
        Vk::SetDebugName(device, descriptorSet.handle, "MegaSet/DescriptorSet");

        Logger::Info("{}\n", "Initialised mega set!");
    }

    u32 MegaSet::WriteSampler(const Vk::Sampler& sampler)
    {
        const auto id = m_samplerID++;

        m_writer.WriteImage
        (
            descriptorSet.handle,
            DescriptorBinding::SAMPLER_BINDING,
            id,
            sampler.handle,
            VK_NULL_HANDLE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_DESCRIPTOR_TYPE_SAMPLER
        );

        return id;
    }

    u32 MegaSet::WriteImage(const Vk::ImageView& imageView, VkImageLayout layout)
    {
        const auto id = m_imageID++;

        m_writer.WriteImage
        (
            descriptorSet.handle,
            DescriptorBinding::SAMPLED_IMAGES_BINDING,
            id,
            VK_NULL_HANDLE,
            imageView.handle,
            layout,
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
        );

        return id;
    }

    void MegaSet::Update(VkDevice device)
    {
        m_writer.Update(device);
        m_writer.Clear();
    }

    void MegaSet::Destroy(VkDevice device)
    {
        vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSet.layout, nullptr);

        Logger::Info("{}\n", "Destroyed mega set!");
    }
}
