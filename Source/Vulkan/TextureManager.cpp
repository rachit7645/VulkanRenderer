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

#include "TextureManager.h"

#include "Texture.h"
#include "Util.h"
#include "DebugUtils.h"
#include "Util/Log.h"
#include "Util/Util.h"

namespace Vk
{
    constexpr u32 MAX_TEXTURE_COUNT = 1 << 16;

    TextureManager::TextureManager(VkPhysicalDevice physicalDevice)
    {
        m_format = Vk::FindSupportedFormat
        (
            physicalDevice,
            std::array{VK_FORMAT_R8G8B8_UNORM, VK_FORMAT_R8G8B8A8_UNORM},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_2_TRANSFER_DST_BIT  |
            VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT |
            VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_FILTER_LINEAR_BIT
        );

        m_formatSRGB = Vk::FindSupportedFormat
        (
            physicalDevice,
            std::array{VK_FORMAT_R8G8B8_SRGB, VK_FORMAT_R8G8B8A8_SRGB},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_2_TRANSFER_DST_BIT  |
            VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_BIT |
            VK_FORMAT_FEATURE_2_SAMPLED_IMAGE_FILTER_LINEAR_BIT
        );
    }

    usize TextureManager::AddTexture
    (
        Vk::MegaSet& megaSet,
        VkDevice device,
        VmaAllocator allocator,
        const std::string_view path,
        Texture::Flags flags
    )
    {
        usize pathHash = std::hash<std::string_view>()(path);

        if (!textureMap.contains(pathHash))
        {
            Vk::Texture texture = {};

            const auto stagingBuffer = texture.LoadFromFile
            (
                device,
                allocator,
                Texture::IsFlagSet(flags, Texture::Flags::IsSRGB) ? m_formatSRGB : m_format,
                path,
                flags
            );

            const auto id = megaSet.WriteImage(texture.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            textureMap.emplace(pathHash, TextureInfo(id, texture));
            m_pendingUploads.emplace_back(texture, stagingBuffer);
        }
        else
        {
            Logger::Debug("Loading cached texture! [Path={}]\n", path);
        }

        return pathHash;
    }

    u32 TextureManager::AddSampler
    (
        Vk::MegaSet& megaSet,
        VkDevice device,
        const VkSamplerCreateInfo& createInfo
    )
    {
        const auto sampler = Vk::Sampler(device, createInfo);
        const auto id      = megaSet.WriteSampler(sampler);

        samplerMap.emplace(id, sampler);

        return id;
    }

    void TextureManager::Update(const Vk::CommandBuffer& cmdBuffer)
    {
        Vk::BeginLabel(cmdBuffer, "Texture Transfer", {0.6117f, 0.8196f, 0.0313f, 1.0f});

        for (auto& [texture, stagingBuffer] : m_pendingUploads)
        {
            texture.UploadToGPU(cmdBuffer, stagingBuffer);
        }

        Vk::EndLabel(cmdBuffer);
    }

    void TextureManager::Clear(VmaAllocator allocator)
    {
        for (auto& buffer : m_pendingUploads | std::views::values)
        {
            buffer.Destroy(allocator);
        }

        m_pendingUploads.clear();
    }

    u32 TextureManager::GetTextureID(usize pathHash) const
    {
        const auto iter = textureMap.find(pathHash);

        if (iter == textureMap.end())
        {
            Logger::Error("Invalid texture path hash! [Hash={}]\n", pathHash);
        }

        return iter->second.textureID;
    }

    const Texture& TextureManager::GetTexture(usize pathHash) const
    {
        const auto iter = textureMap.find(pathHash);

        if (iter == textureMap.end())
        {
            Logger::Error("Invalid texture path hash! [Hash={}]\n", pathHash);
        }

        return iter->second.texture;
    }

    const Vk::Sampler& TextureManager::GetSampler(u32 id) const
    {
        const auto iter = samplerMap.find(id);

        if (iter == samplerMap.end())
        {
            Logger::Error("Invalid sampler ID! [ID={}]\n", id);
        }

        return iter->second;
    }

    void TextureManager::Destroy(VkDevice device, VmaAllocator allocator)
    {
        for (auto& [textureID, texture] : textureMap | std::views::values)
        {
            texture.Destroy(device, allocator);
        }

        for (const auto& sampler : samplerMap | std::views::values)
        {
            sampler.Destroy(device);
        }

        Logger::Info("{}\n", "Destroyed texture manager!");
    }
}