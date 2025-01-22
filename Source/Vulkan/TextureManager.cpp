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
#include "Util/Log.h"
#include "Util/Util.h"

namespace Vk
{
    constexpr u32          MAX_TEXTURE_COUNT   = 1 << 16;
    constexpr VkDeviceSize STAGING_BUFFER_SIZE = 64 * 1024 * 1024;

    TextureManager::TextureManager(UNUSED VkDevice device, VmaAllocator allocator)
    {
        m_stagingBuffer = Vk::Buffer
        (
            allocator,
            STAGING_BUFFER_SIZE,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            VMA_MEMORY_USAGE_AUTO
        );

        #ifdef ENGINE_DEBUG
        VkDebugUtilsObjectNameInfoEXT nameInfo =
        {
            .sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .pNext        = nullptr,
            .objectType   = VK_OBJECT_TYPE_BUFFER,
            .objectHandle = std::bit_cast<u64>(m_stagingBuffer.handle),
            .pObjectName  = "TextureManager/StagingBuffer"
        };

        vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
        #endif
    }

    usize TextureManager::AddTexture
    (
        Vk::MegaSet& megaSet,
        const Vk::Context& context,
        const std::string_view path,
        Texture::Flags flags
    )
    {
        usize pathHash = std::hash<std::string_view>()(path);

        if (!textureMap.contains(pathHash))
        {
            const auto texture = Vk::Texture(context, m_stagingBuffer, path, flags);
            const auto id      = megaSet.WriteImage(texture.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            textureMap.emplace(pathHash, TextureInfo(id, texture));
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
        m_stagingBuffer.Destroy(allocator);

        for (auto& [textureID, texture] : textureMap | std::views::values)
        {
            texture.Destroy(device, allocator);
        }

        for (auto& [id, sampler] : samplerMap)
        {
            sampler.Destroy(device);
        }

        Logger::Info("{}\n", "Destroyed texture manager!");
    }
}