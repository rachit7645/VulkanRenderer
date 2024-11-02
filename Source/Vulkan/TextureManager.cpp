//    Copyright 2023 - 2024 Rachit Khandelwal
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.

#include "TextureManager.h"

#include "Texture.h"
#include "Util.h"
#include "Util/Log.h"
#include "Util/Util.h"

namespace Vk
{
    constexpr u32 MAX_TEXTURE_COUNT = 1 << 16;

    TextureManager::TextureManager(VkDevice device, const VkPhysicalDeviceLimits& deviceLimits)
    {
        CreatePool(device);
        CreateLayout(device, deviceLimits);
        CreateSet(device, deviceLimits);

        Logger::Info("{}\n", "Initialised texture manager!");
    }

    usize TextureManager::AddTexture(const Vk::Context& context, const std::string_view path, Texture::Flags flags)
    {
        usize pathHash = std::hash<std::string_view>()(path);

        if (!textureMap.contains(pathHash))
        {
            const u32  id      = m_lastID++;
            const auto texture = Vk::Texture(context, path, flags);
            textureMap.emplace(pathHash, TextureInfo(id, texture));

            m_writer.WriteImage
            (
                textureSet.handle,
                0,
                id,
                VK_NULL_HANDLE,
                texture.imageView.handle,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
            );
        }
        else
        {
            Logger::Debug("Loading cached texture! [Path={}]\n", path);
        }

        return pathHash;
    }

    usize TextureManager::AddTexture(const Vk::Texture& texture)
    {
        const u32 id = m_lastID++;

        usize pathHash = std::hash<std::string>()("DummyTexturePath" + std::to_string(id));
        textureMap.emplace(pathHash, TextureInfo(id, texture));

        m_writer.WriteImage
        (
            textureSet.handle,
            0,
            id,
            VK_NULL_HANDLE,
            texture.imageView.handle,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
        );

        return pathHash;
    }

    void TextureManager::Update(VkDevice device)
    {
        m_writer.Update(device);
        m_writer.Clear();
    }

    u32 TextureManager::GetID(usize pathHash) const
    {
        const auto iter = textureMap.find(pathHash);

        if (iter == textureMap.end())
        {
            Logger::Error("Invalid path hash! [Hash={}]\n", pathHash);
        }

        return iter->second.textureID;
    }

    const Texture& TextureManager::GetTexture(usize pathHash) const
    {
        const auto iter = textureMap.find(pathHash);

        if (iter == textureMap.end())
        {
            Logger::Error("Invalid path hash! [Hash={}]\n", pathHash);
        }

        return iter->second.texture;
    }

    void TextureManager::CreatePool(VkDevice device)
    {
        constexpr VkDescriptorPoolSize samplerPoolSize =
        {
            .type            = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 1
        };

        const VkDescriptorPoolCreateInfo poolCreateInfo =
        {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext         = nullptr,
            .flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
            .maxSets       = static_cast<u32>(samplerPoolSize.descriptorCount),
            .poolSizeCount = 1,
            .pPoolSizes    = &samplerPoolSize
        };

        Vk::CheckResult(vkCreateDescriptorPool(
            device,
            &poolCreateInfo,
            nullptr,
            &m_texturePool),
            "Failed to create texture descriptor pool!"
        );
    }

    void TextureManager::CreateLayout(VkDevice device, const VkPhysicalDeviceLimits& deviceLimits)
    {
        VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
                                         VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT           |
                                         VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

        const VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags =
        {
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            .pNext         = nullptr,
            .bindingCount  = 1,
            .pBindingFlags = &flags
        };

        auto maxTextureCount = std::min(deviceLimits.maxDescriptorSetSampledImages, MAX_TEXTURE_COUNT);

        const VkDescriptorSetLayoutBinding textureArrayBinding =
        {
            .binding            = 0,
            .descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount    = maxTextureCount,
            .stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr
        };

        const VkDescriptorSetLayoutCreateInfo createInfo =
        {
            .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext        = &bindingFlags,
            .flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
            .bindingCount = 1,
            .pBindings    = &textureArrayBinding
        };

        Vk::CheckResult(vkCreateDescriptorSetLayout(
            device,
            &createInfo,
            nullptr,
            &textureSet.layout),
            "Failed to create texture descriptor set layout!"
        );
    }

    void TextureManager::CreateSet(VkDevice device, const VkPhysicalDeviceLimits& deviceLimits)
    {
        auto maxTextureCount = std::min(deviceLimits.maxDescriptorSetSampledImages, MAX_TEXTURE_COUNT);

        VkDescriptorSetVariableDescriptorCountAllocateInfo setCounts =
        {
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
            .pNext              = nullptr,
            .descriptorSetCount = 1,
            .pDescriptorCounts  = &maxTextureCount
        };

        VkDescriptorSetAllocateInfo allocInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext              = &setCounts,
            .descriptorPool     = m_texturePool,
            .descriptorSetCount = 1,
            .pSetLayouts        = &textureSet.layout
        };

        Vk::CheckResult(vkAllocateDescriptorSets(device, &allocInfo, &textureSet.handle), "Failed to allocate texture descriptor set!");
    }

    void TextureManager::Destroy(VkDevice device, VmaAllocator allocator)
    {
        vkDestroyDescriptorPool(device, m_texturePool, nullptr);
        vkDestroyDescriptorSetLayout(device, textureSet.layout, nullptr);

        for (auto& [textureID, texture] : textureMap | std::views::values)
        {
            texture.Destroy(device, allocator);
        }

        Logger::Info("{}\n", "Destroyed texture manager!");
    }
}