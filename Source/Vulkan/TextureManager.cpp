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
#include "Util/Types.h"

namespace Vk
{
    Vk::TextureID TextureManager::AddTexture
    (
        VmaAllocator allocator,
        Util::DeletionQueue& deletionQueue,
        const std::string_view path
    )
    {
        const Vk::TextureID id = std::hash<std::string_view>()(path);

        if (m_textureMap.contains(id))
        {
            return id;
        }

        m_futuresMap.emplace(id, m_executor.async([this, allocator, &deletionQueue, path = std::string(path)] ()
        {
            return m_imageUploader.LoadImageFromFile(allocator, deletionQueue, path);
        }));

        m_textureMap.emplace(id, TextureInfo{
            .name         = Util::Files::GetNameWithoutExtension(path),
            .texture      = {},
            .descriptorID = 0,
            .isLoaded     = false
        });

        return id;
    }

    Vk::TextureID TextureManager::AddTexture
    (
        VmaAllocator allocator,
        Util::DeletionQueue& deletionQueue,
        const std::string_view name,
        VkFormat format,
        const void* data,
        u32 width,
        u32 height
    )
    {
        const Vk::TextureID id = std::hash<std::string_view>()(name);

        if (m_textureMap.contains(id))
        {
            return id;
        }

        m_futuresMap.emplace(id, m_executor.async([this, allocator, &deletionQueue, format, data, width, height] ()
        {
            return m_imageUploader.LoadImageFromMemory
            (
                allocator,
                deletionQueue,
                format,
                data,
                width,
                height
            );
        }));

        m_textureMap.emplace(id, TextureInfo{
            .name         = name.data(),
            .texture      = {},
            .descriptorID = 0,
            .isLoaded     = false
        });

        return id;
    }

    Vk::TextureID TextureManager::AddTexture
    (
        Vk::MegaSet& megaSet,
        VkDevice device,
        const std::string_view name,
        const Vk::Texture& texture
    )
    {
        const Vk::TextureID id = std::hash<std::string_view>()(name);

        if (m_textureMap.contains(id))
        {
            return id;
        }

        const auto descriptorID = megaSet.WriteSampledImage(texture.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        m_textureMap.emplace(id, TextureInfo{
            .name         = name.data(),
            .texture      = texture,
            .descriptorID = descriptorID,
            .isLoaded     = true
        });

        Vk::SetDebugName(device, texture.image.handle,     name);
        Vk::SetDebugName(device, texture.imageView.handle, name.data() + std::string("_View"));

        Logger::Debug("Added texture! [Name={}]\n", name);

        return id;
    }

    Vk::DescriptorID TextureManager::AddSampler
    (
        Vk::MegaSet& megaSet,
        VkDevice device,
        const VkSamplerCreateInfo& createInfo
    )
    {
        const auto sampler = Vk::Sampler(device, createInfo);
        const auto id      = megaSet.WriteSampler(sampler);

        m_samplerMap.emplace(id, sampler);

        return id;
    }

    void TextureManager::Update(const Vk::CommandBuffer& cmdBuffer, VkDevice device, Vk::MegaSet& megaSet)
    {
        if (!HasPendingUploads())
        {
            return;
        }

        m_executor.wait_for_all();

        for (auto& [id, info] : m_textureMap)
        {
            if (m_futuresMap.contains(id))
            {
                auto& future = m_futuresMap.at(id);

                if (!future.valid())
                {
                    Logger::Error("Future is not valid! [ID={}]", id);
                }

                future.wait();

                info.texture.image = future.get();

                info.texture.imageView = Vk::ImageView
                (
                    device,
                    info.texture.image,
                    VK_IMAGE_VIEW_TYPE_2D,
                    VkImageSubresourceRange{
                        .aspectMask     = info.texture.image.aspect,
                        .baseMipLevel   = 0,
                        .levelCount     = info.texture.image.mipLevels,
                        .baseArrayLayer = 0,
                        .layerCount     = info.texture.image.arrayLayers
                    }
                );

                info.descriptorID = megaSet.WriteSampledImage(info.texture.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

                info.isLoaded = true;

                Vk::SetDebugName(device, info.texture.image.handle,     info.name);
                Vk::SetDebugName(device, info.texture.imageView.handle, info.name + "_View");

                Logger::Debug("Loaded texture! [Name={}]\n", info.name);
            }
        }

        m_futuresMap.clear();

        Vk::BeginLabel(cmdBuffer, "Texture Transfer", {0.6117f, 0.8196f, 0.0313f, 1.0f});

        m_imageUploader.FlushUploads(cmdBuffer);

        Vk::EndLabel(cmdBuffer);
    }

    const TextureManager::TextureInfo& TextureManager::GetTextureInfo(Vk::TextureID id) const
    {
        const auto iter = m_textureMap.find(id);

        if (iter == m_textureMap.end())
        {
            Logger::Error("Invalid texture id! [ID={}]\n", id);
        }

        if (!iter->second.isLoaded)
        {
            Logger::Error("Texture not yet loaded! [ID={}]\n", id);
        }

        return iter->second;
    }

    const Vk::Sampler& TextureManager::GetSampler(Vk::DescriptorID id) const
    {
        const auto iter = m_samplerMap.find(id);

        if (iter == m_samplerMap.end())
        {
            Logger::Error("Invalid sampler ID! [ID={}]\n", id);
        }

        return iter->second;
    }

    void TextureManager::DestroyTexture
    (
        Vk::TextureID id,
        VkDevice device,
        VmaAllocator allocator,
        Vk::MegaSet& megaSet,
        Util::DeletionQueue& deletionQueue
    )
    {
        const auto iter = m_textureMap.find(id);

        if (iter == m_textureMap.end())
        {
            return;
        }

        deletionQueue.PushDeletor([&megaSet, device, allocator, textureInfo = iter->second] () mutable
        {
            textureInfo.texture.Destroy(device, allocator);
            megaSet.FreeSampledImage(textureInfo.descriptorID);
        });

        m_textureMap.erase(iter);
    }

    void TextureManager::ImGuiDisplay()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Texture Manager"))
            {
                for (const auto& [id, textureInfo] : m_textureMap)
                {
                    const auto& [name, texture, descriptorID, isLoaded] = textureInfo;

                    if (!isLoaded)
                    {
                        continue;
                    }

                    if (ImGui::TreeNode(std::bit_cast<void*>(id), name.c_str()))
                    {
                        ImGui::Text("Descriptor Index | %u", descriptorID);
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

                        ImGui::Image(descriptorID, imageSize);

                        ImGui::TreePop();
                    }

                    ImGui::Separator();
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }

    bool TextureManager::HasPendingUploads()
    {
        return m_imageUploader.HasPendingUploads() || !m_futuresMap.empty();
    }

    void TextureManager::Destroy(VkDevice device, VmaAllocator allocator)
    {
        for (auto& textureInfo : m_textureMap | std::views::values)
        {
            if (textureInfo.isLoaded)
            {
                textureInfo.texture.Destroy(device, allocator);
            }
        }

        for (const auto& sampler : m_samplerMap | std::views::values)
        {
            sampler.Destroy(device);
        }

        m_textureMap.clear();
        m_samplerMap.clear();
        m_futuresMap.clear();

        m_imageUploader.Clear();
    }
}