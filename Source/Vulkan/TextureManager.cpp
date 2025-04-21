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
    TextureManager::TextureManager(const Vk::FormatHelper& formatHelper)
        : m_formatHelper(formatHelper)
    {
    }

    u32 TextureManager::AddTexture
    (
        Vk::MegaSet& megaSet,
        VkDevice device,
        VmaAllocator allocator,
        const std::string_view path
    )
    {
        const usize pathHash = std::hash<std::string_view>()(path);

        if (m_nameHashToTextureIDMap.contains(pathHash))
        {
            return m_nameHashToTextureIDMap.at(pathHash);
        }

        Vk::Texture     texture = {};
        Texture::Upload upload  = {};

        if (Engine::Files::GetExtension(path) == ".hdr")
        {
            upload = texture.LoadFromFileHDR
            (
                device,
                allocator,
                m_formatHelper.textureFormatHDR,
                path
            );
        }
        else
        {
            upload = texture.LoadFromFile
            (
                device,
                allocator,
                path
            );
        }

        const auto id = megaSet.WriteSampledImage(texture.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        textureMap.emplace
        (
            id,
            TextureInfo(
                Engine::Files::GetNameWithoutExtension(path),
                texture
            )
        );

        m_nameHashToTextureIDMap.emplace(pathHash, id);

        m_pendingUploads.emplace_back(texture, upload);

        return id;
    }

    u32 TextureManager::AddTexture
    (
        Vk::MegaSet& megaSet,
        VkDevice device,
        VmaAllocator allocator,
        const std::string_view name,
        const std::span<const u8> data,
        const glm::uvec2 size,
        VkFormat format
    )
    {
        const usize nameHash = std::hash<std::string_view>()(name);

        if (m_nameHashToTextureIDMap.contains(nameHash))
        {
            return m_nameHashToTextureIDMap.at(nameHash);
        }

        Vk::Texture texture = {};

        const auto upload = texture.LoadFromMemory
        (
            device,
            allocator,
            format,
            data,
            size
        );

        const auto id = megaSet.WriteSampledImage(texture.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        textureMap.emplace
        (
            id,
            TextureInfo(
                name.data(),
                texture
            )
        );

        m_nameHashToTextureIDMap.emplace(nameHash, id);

        m_pendingUploads.emplace_back(texture, upload);

        Vk::SetDebugName(device, texture.image.handle,     name);
        Vk::SetDebugName(device, texture.imageView.handle, name.data() + std::string("_View"));

        return id;
    }

    u32 TextureManager::AddTexture
    (
        Vk::MegaSet& megaSet,
        VkDevice device,
        const std::string_view name,
        const Vk::Texture& texture
    )
    {
        const usize nameHash = std::hash<std::string_view>()(name);

        if (m_nameHashToTextureIDMap.contains(nameHash))
        {
            return m_nameHashToTextureIDMap.at(nameHash);
        }

        const auto id = megaSet.WriteSampledImage(texture.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        textureMap.emplace
        (
            id,
            TextureInfo(
                name.data(),
                texture
            )
        );

        m_nameHashToTextureIDMap.emplace(nameHash, id);

        Vk::SetDebugName(device, texture.image.handle,     name);
        Vk::SetDebugName(device, texture.imageView.handle, name.data() + std::string("_View"));

        return id;
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
        if (!HasPendingUploads())
        {
            return;
        }

        Vk::BeginLabel(cmdBuffer, "Texture Transfer", {0.6117f, 0.8196f, 0.0313f, 1.0f});

        for (auto& [texture, upload] : m_pendingUploads)
        {
            texture.UploadToGPU(cmdBuffer, upload);
        }

        Vk::EndLabel(cmdBuffer);
    }

    void TextureManager::ClearUploads(VmaAllocator allocator)
    {
        for (auto& [buffer, _] : m_pendingUploads | std::views::values)
        {
            buffer.Destroy(allocator);
        }

        m_pendingUploads.clear();
    }

    const Texture& TextureManager::GetTexture(u32 id) const
    {
        const auto iter = textureMap.find(id);

        if (iter == textureMap.end())
        {
            Logger::Error("Invalid texture id! [ID={}]\n", id);
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

    void TextureManager::DestroyTexture(VkDevice device, VmaAllocator allocator, u32 id)
    {
        const auto iter = textureMap.find(id);

        if (iter == textureMap.end())
        {
            return;
        }

        for (auto it = m_nameHashToTextureIDMap.begin(); it != m_nameHashToTextureIDMap.end();)
        {
            if (it->second == iter->first)
            {
                it = m_nameHashToTextureIDMap.erase(it);
            }
            else
            {
                ++it;
            }
        }

        iter->second.texture.Destroy(device, allocator);
        textureMap.erase(iter);
    }

    void TextureManager::ImGuiDisplay()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Texture Manager"))
            {
                for (const auto& [nameHash, textureID] : m_nameHashToTextureIDMap)
                {
                    const auto& [name, texture] = textureMap.at(textureID);

                    if (ImGui::TreeNode(std::bit_cast<void*>(nameHash), name.c_str()))
                    {
                        ImGui::Text("Descriptor Index | %u", textureID);
                        ImGui::Text("Width            | %u", texture.image.width);
                        ImGui::Text("Height           | %u", texture.image.height);
                        ImGui::Text("Depth            | %u", texture.image.depth);
                        ImGui::Text("Mipmap Levels    | %u", texture.image.mipLevels);
                        ImGui::Text("Array Layers     | %u", texture.image.arrayLayers);
                        ImGui::Text("Format           | %s", string_VkFormat(texture.image.format));
                        ImGui::Text("Usage            | %s", string_VkImageUsageFlags(texture.image.usage).c_str());

                        ImGui::Separator();

                        const f32 originalWidth  = static_cast<f32>(texture.image.width);
                        const f32 originalHeight = static_cast<f32>(texture.image.height);

                        constexpr f32 MAX_SIZE = 512.0f;

                        // Maintain aspect ratio
                        const f32  scale     = std::min(MAX_SIZE / originalWidth, MAX_SIZE / originalHeight);
                        const auto imageSize = ImVec2(originalWidth * scale, originalHeight * scale);

                        ImGui::Image(textureID, imageSize);

                        ImGui::TreePop();
                    }

                    ImGui::Separator();
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }

    bool TextureManager::HasPendingUploads() const
    {
        return !m_pendingUploads.empty();
    }

    void TextureManager::Destroy(VkDevice device, VmaAllocator allocator)
    {
        for (auto& [_, texture] : textureMap | std::views::values)
        {
            texture.Destroy(device, allocator);
        }

        for (const auto& sampler : samplerMap | std::views::values)
        {
            sampler.Destroy(device);
        }

        textureMap.clear();
        samplerMap.clear();

        m_nameHashToTextureIDMap.clear();
        m_pendingUploads.clear();

        Logger::Info("{}\n", "Destroyed texture manager!");
    }
}