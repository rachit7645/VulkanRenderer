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
    MegaSet::MegaSet(const Vk::Context& context)
    {
        constexpr u32 MAX_SAMPLERS       = 1 << 8;
        constexpr u32 MAX_SAMPLED_IMAGES = 1 << 14;
        constexpr u32 MAX_STORAGE_IMAGES = 1 << 10;
        
        const auto maxSamplers      = std::min(context.physicalDeviceVulkan12Properties.maxDescriptorSetUpdateAfterBindSamplers,      MAX_SAMPLERS);
        const auto maxSampledImages = std::min(context.physicalDeviceVulkan12Properties.maxDescriptorSetUpdateAfterBindSampledImages, MAX_SAMPLED_IMAGES);
        const auto maxStorageImages = std::min(context.physicalDeviceVulkan12Properties.maxDescriptorSetUpdateAfterBindStorageImages, MAX_STORAGE_IMAGES);

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
            },
            VkDescriptorPoolSize
            {
                .type            = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .descriptorCount = maxStorageImages
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
            context.device,
            &poolCreateInfo,
            nullptr,
            &m_descriptorPool),
            "Failed to create mega set descriptor pool!"
        );

        constexpr std::array<VkDescriptorBindingFlags, DescriptorBinding::MEGA_SET_BINDINGS_COUNT> bindingFlags =
        {
            // Samplers
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT,
            // Sampled images
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT,
            // Storage images
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT
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
                .binding            = DescriptorBinding::MEGA_SET_SAMPLER_BINDING,
                .descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLER,
                .descriptorCount    = maxSamplers,
                .stageFlags         = VK_SHADER_STAGE_ALL,
                .pImmutableSamplers = nullptr
            },
            VkDescriptorSetLayoutBinding
            {
                .binding            = DescriptorBinding::MEGA_SET_SAMPLED_IMAGES_BINDING,
                .descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .descriptorCount    = maxSampledImages,
                .stageFlags         = VK_SHADER_STAGE_ALL,
                .pImmutableSamplers = nullptr
            },
            VkDescriptorSetLayoutBinding
            {
                .binding            = DescriptorBinding::MEGA_SET_STORAGE_IMAGES_BINDING,
                .descriptorType     = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .descriptorCount    = maxStorageImages,
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
            context.device,
            &createInfo,
            nullptr,
            &descriptorLayout),
            "Failed to create mega set layout!"
        );

        const VkDescriptorSetAllocateInfo allocInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext              = nullptr,
            .descriptorPool     = m_descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts        = &descriptorLayout
        };

        Vk::CheckResult(vkAllocateDescriptorSets(
            context.device,
            &allocInfo,
            &descriptorSet),
            "Failed to allocate mega set"
        );

        m_samplerAllocator      = Vk::DescriptorAllocator(maxSamplers);
        m_sampledImageAllocator = Vk::DescriptorAllocator(maxSampledImages);
        m_storageImageAllocator = Vk::DescriptorAllocator(maxStorageImages);

        Vk::SetDebugName(context.device, m_descriptorPool, "MegaSet/DescriptorPool");
        Vk::SetDebugName(context.device, descriptorLayout, "MegaSet/DescriptorLayout");
        Vk::SetDebugName(context.device, descriptorSet,    "MegaSet/DescriptorSet");

        Logger::Info("{}\n", "Initialised mega set!");
    }

    u32 MegaSet::WriteSampler(const Vk::Sampler& sampler)
    {
        const auto id = m_samplerAllocator.Allocate();

        m_writer.WriteImage
        (
            descriptorSet,
            DescriptorBinding::MEGA_SET_SAMPLER_BINDING,
            id,
            sampler.handle,
            VK_NULL_HANDLE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_DESCRIPTOR_TYPE_SAMPLER
        );

        return id;
    }

    u32 MegaSet::WriteSampledImage(const Vk::ImageView& imageView, VkImageLayout layout)
    {
        const auto id = m_sampledImageAllocator.Allocate();

        m_writer.WriteImage
        (
            descriptorSet,
            DescriptorBinding::MEGA_SET_SAMPLED_IMAGES_BINDING,
            id,
            VK_NULL_HANDLE,
            imageView.handle,
            layout,
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
        );

        return id;
    }

    u32 MegaSet::WriteStorageImage(const Vk::ImageView& imageView)
    {
        const auto id = m_storageImageAllocator.Allocate();

        m_writer.WriteImage
        (
            descriptorSet,
            DescriptorBinding::MEGA_SET_STORAGE_IMAGES_BINDING,
            id,
            VK_NULL_HANDLE,
            imageView.handle,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
        );

        return id;
    }

    void MegaSet::FreeSampler(u32 id)
    {
        m_samplerAllocator.Free(id);
    }

    void MegaSet::FreeSampledImage(u32 id)
    {
        m_sampledImageAllocator.Free(id);
    }

    void MegaSet::FreeStorageImage(u32 id)
    {
        m_storageImageAllocator.Free(id);
    }

    void MegaSet::Update(VkDevice device)
    {
        m_writer.Update(device);
        m_writer.Clear();
    }

    void MegaSet::ImGuiDisplay()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("MegaSet"))
            {
                ImGui::Text
                (
                    "%-8s | %-14s | %-10s | %-10s | %-4s | %-8s",
                    "Binding",
                    "Type",
                    "Allocated",
                    "Free Slots",
                    "Used",
                    "Max Count"
                );

                ImGui::Separator();

                ImGui::Text
                (
                    "%-8u | %-14s | %-10u | %-10u | %-4u | %-8u",
                    DescriptorBinding::MEGA_SET_SAMPLER_BINDING,
                    "Sampler",
                    m_samplerAllocator.GetAllocatedCount(),
                    m_samplerAllocator.GetFreeSlotCount(),
                    m_samplerAllocator.GetUsedCount(),
                    m_samplerAllocator.GetMaxCount()
                );

                ImGui::Separator();

                ImGui::Text
                (
                    "%-8u | %-14s | %-10u | %-10u | %-4u | %-8u",
                    DescriptorBinding::MEGA_SET_SAMPLED_IMAGES_BINDING,
                    "Sampled Image",
                    m_sampledImageAllocator.GetAllocatedCount(),
                    m_sampledImageAllocator.GetFreeSlotCount(),
                    m_sampledImageAllocator.GetUsedCount(),
                    m_sampledImageAllocator.GetMaxCount()
                );

                ImGui::Separator();

                ImGui::Text
                (
                    "%-8u | %-14s | %-10u | %-10u | %-4u | %-8u",
                    DescriptorBinding::MEGA_SET_STORAGE_IMAGES_BINDING,
                    "Storage Image",
                    m_storageImageAllocator.GetAllocatedCount(),
                    m_storageImageAllocator.GetFreeSlotCount(),
                    m_storageImageAllocator.GetUsedCount(),
                    m_storageImageAllocator.GetMaxCount()
                );

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }

    void MegaSet::Destroy(VkDevice device)
    {
        vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorLayout, nullptr);

        Logger::Info("{}\n", "Destroyed mega set!");
    }
}