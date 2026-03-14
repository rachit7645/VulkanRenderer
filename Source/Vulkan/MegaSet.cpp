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

#include "MegaSet.h"

#include <volk/volk.h>

#include "DebugUtils.h"
#include "Util.h"
#include "Util/Log.h"
#include "Externals/ImGui.h"

namespace Vk
{
    enum class DescriptorBinding : u32
    {
        Samplers      = 0,
        SampledImages = 1,
        StorageImages = 2,
        Count
    };

    MegaSet::MegaSet(const Vk::Context& context)
    {
        constexpr u32 MAX_SAMPLERS       = 1 << 8;
        constexpr u32 MAX_SAMPLED_IMAGES = 1 << 14;
        constexpr u32 MAX_STORAGE_IMAGES = 1 << 10;
        
        const auto maxSamplers      = std::min(context.physicalDeviceVulkan12Properties.maxPerStageDescriptorUpdateAfterBindSamplers,      MAX_SAMPLERS);
        const auto maxSampledImages = std::min(context.physicalDeviceVulkan12Properties.maxPerStageDescriptorUpdateAfterBindSampledImages, MAX_SAMPLED_IMAGES);
        const auto maxStorageImages = std::min(context.physicalDeviceVulkan12Properties.maxPerStageDescriptorUpdateAfterBindStorageImages, MAX_STORAGE_IMAGES);

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

        constexpr std::array<VkDescriptorBindingFlags, std::to_underlying(DescriptorBinding::Count)> bindingFlags =
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
                .binding            = std::to_underlying(DescriptorBinding::Samplers),
                .descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLER,
                .descriptorCount    = maxSamplers,
                .stageFlags         = VK_SHADER_STAGE_ALL,
                .pImmutableSamplers = nullptr
            },
            VkDescriptorSetLayoutBinding
            {
                .binding            = std::to_underlying(DescriptorBinding::SampledImages),
                .descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .descriptorCount    = maxSampledImages,
                .stageFlags         = VK_SHADER_STAGE_ALL,
                .pImmutableSamplers = nullptr
            },
            VkDescriptorSetLayoutBinding
            {
                .binding            = std::to_underlying(DescriptorBinding::StorageImages),
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
    }

    Vk::DescriptorID MegaSet::WriteSampler(const Vk::Sampler& sampler)
    {
        const auto id = m_samplerAllocator.Allocate();

        m_writer.WriteImage
        (
            descriptorSet,
            std::to_underlying(DescriptorBinding::Samplers),
            id,
            sampler.handle,
            VK_NULL_HANDLE,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_DESCRIPTOR_TYPE_SAMPLER
        );

        return id;
    }

    Vk::DescriptorID MegaSet::WriteSampledImage(const Vk::ImageView& imageView)
    {
        const auto id = m_sampledImageAllocator.Allocate();

        m_writer.WriteImage
        (
            descriptorSet,
            std::to_underlying(DescriptorBinding::SampledImages),
            id,
            VK_NULL_HANDLE,
            imageView.handle,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
        );

        return id;
    }

    Vk::DescriptorID MegaSet::WriteStorageImage(const Vk::ImageView& imageView)
    {
        const auto id = m_storageImageAllocator.Allocate();

        m_writer.WriteImage
        (
            descriptorSet,
            std::to_underlying(DescriptorBinding::StorageImages),
            id,
            VK_NULL_HANDLE,
            imageView.handle,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
        );

        return id;
    }

    void MegaSet::FreeSampler(Vk::DescriptorID id)
    {
        m_samplerAllocator.Free(id);
    }

    void MegaSet::FreeSampledImage(Vk::DescriptorID id)
    {
        m_sampledImageAllocator.Free(id);
    }

    void MegaSet::FreeStorageImage(Vk::DescriptorID id)
    {
        m_storageImageAllocator.Free(id);
    }

    void MegaSet::Update(VkDevice device)
    {
        m_writer.Update(device);
    }

    void MegaSet::ImGuiDisplay()
    {
        if (ImGui::CollapsingHeader("MegaSet"))
        {
            constexpr auto ALLOCATOR_STATISTICS_FORMAT = "%-8u | %-14s | %-10u | %-10u | %-4u | %-8u";

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
                ALLOCATOR_STATISTICS_FORMAT,
                std::to_underlying(DescriptorBinding::Samplers),
                "Sampler",
                m_samplerAllocator.GetAllocatedCount(),
                m_samplerAllocator.GetFreeSlotCount(),
                m_samplerAllocator.GetUsedCount(),
                m_samplerAllocator.GetMaxCount()
            );

            ImGui::Separator();

            ImGui::Text
            (
                ALLOCATOR_STATISTICS_FORMAT,
                std::to_underlying(DescriptorBinding::SampledImages),
                "Sampled Image",
                m_sampledImageAllocator.GetAllocatedCount(),
                m_sampledImageAllocator.GetFreeSlotCount(),
                m_sampledImageAllocator.GetUsedCount(),
                m_sampledImageAllocator.GetMaxCount()
            );

            ImGui::Separator();

            ImGui::Text
            (
                ALLOCATOR_STATISTICS_FORMAT,
                std::to_underlying(DescriptorBinding::StorageImages),
                "Storage Image",
                m_storageImageAllocator.GetAllocatedCount(),
                m_storageImageAllocator.GetFreeSlotCount(),
                m_storageImageAllocator.GetUsedCount(),
                m_storageImageAllocator.GetMaxCount()
            );
        }
    }

    void MegaSet::Destroy(VkDevice device)
    {
        vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorLayout, nullptr);
    }
}