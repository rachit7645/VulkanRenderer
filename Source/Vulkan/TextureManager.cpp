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
#include "Util/Visitor.h"

namespace Vk
{
    Vk::TextureID TextureManager::AddTexture
    (
        VmaAllocator allocator,
        Util::DeletionQueue& deletionQueue,
        const Vk::ImageUpload& upload
    )
    {
        const auto [name, nameID] = std::visit(Util::Visitor{
            [] (const Vk::ImageUploadFile& file)
            {
                return std::make_pair(Util::Files::GetNameWithoutExtension(file.path), file.path);
            },
            [] (const Vk::ImageUploadMemory& memory)
            {
                return std::make_pair(memory.name, memory.name);
            },
            [] (const Vk::ImageUploadRawMemory& rawMemory)
            {
                return std::make_pair(rawMemory.name, rawMemory.name);
            }
        }, upload.source);

        const Vk::TextureID id = std::hash<std::string_view>()(nameID);

        auto iter = m_textureMap.find(id);

        if (iter != m_textureMap.end())
        {
            ++iter->second.referenceCount;

            return id;
        }

        m_futuresMap.emplace(id, m_executor.async([this, allocator, &deletionQueue, upload] ()
        {
            return m_imageUploader.LoadImage(allocator, deletionQueue, upload);
        }));

        m_textureMap.emplace(id, TextureInfo{
            .texture = Vk::Texture{
                .name           = name,
                .image          = {},
                .imageView      = {},
                .descriptorID   = 0,
                .isLoaded       = false
            },
            .referenceCount = 1
        });

        return id;
    }

    Vk::TextureID TextureManager::AddTexture
    (
        Vk::MegaSet& megaSet,
        VkDevice device,
        const std::string_view name,
        const Vk::Image& image,
        const Vk::ImageView& imageView
    )
    {
        const Vk::TextureID id = std::hash<std::string_view>()(name);

        if (m_textureMap.contains(id))
        {
            return id;
        }

        const auto descriptorID = megaSet.WriteSampledImage(imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        m_textureMap.emplace(id, TextureInfo{
            .texture = Vk::Texture{
                .name           = name.data(),
                .image          = image,
                .imageView      = imageView,
                .descriptorID   = descriptorID,
                .isLoaded       = true
            },
            .referenceCount = 1
        });

        Vk::SetDebugName(device, image.handle,     name);
        Vk::SetDebugName(device, imageView.handle, name.data() + std::string("_View"));

        Logger::Debug("Added texture! [Name={}]\n", name);

        return id;
    }

    Vk::SamplerID TextureManager::AddSampler
    (
        Vk::MegaSet& megaSet,
        VkDevice device,
        const VkSamplerCreateInfo& createInfo
    )
    {
        const auto id = std::hash<VkSamplerCreateInfo>()(createInfo);

        if (m_samplerMap.contains(id))
        {
            return id;
        }

        Vk::Sampler sampler = {};

        Vk::CheckResult(vkCreateSampler(
            device,
            &createInfo,
            nullptr,
            &sampler.handle),
            "Failed to create sampler!"
        );

        sampler.descriptorID = megaSet.WriteSampler(sampler);

        Vk::SetDebugName(device, sampler.handle, fmt::format("Sampler/{}", id));

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
            auto& [texture, referenceCount] = info;

            if (texture.isLoaded)
            {
                continue;
            }

            if (referenceCount == 0)
            {
                return;
            }

            auto iter = m_futuresMap.find(id);

            if (iter == m_futuresMap.end())
            {
                continue;
            }

            auto& future = iter->second;

            if (!future.valid())
            {
                Logger::Error("Future is not valid! [ID={}]", id);
            }

            future.wait();

            texture.image = future.get();

            texture.imageView = Vk::ImageView
            (
                device,
                texture.image,
                VK_IMAGE_VIEW_TYPE_2D,
                VkImageSubresourceRange{
                    .aspectMask     = texture.image.aspect,
                    .baseMipLevel   = 0,
                    .levelCount     = texture.image.mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount     = texture.image.arrayLayers
                }
            );

            texture.descriptorID = megaSet.WriteSampledImage(texture.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            texture.isLoaded = true;

            Vk::SetDebugName(device, texture.image.handle,     texture.name);
            Vk::SetDebugName(device, texture.imageView.handle, texture.name + "_View");

            Logger::Debug("Loaded texture! [Name={}]\n", texture.name);
        }

        m_futuresMap.clear();

        Vk::BeginLabel(cmdBuffer, "Texture Transfer", {0.6117f, 0.8196f, 0.0313f, 1.0f});

        m_imageUploader.FlushUploads(cmdBuffer);

        Vk::EndLabel(cmdBuffer);
    }

    const Vk::Texture& TextureManager::GetTexture(Vk::TextureID id) const
    {
        const auto iter = m_textureMap.find(id);

        if (iter == m_textureMap.end())
        {
            Logger::Error("Invalid texture id! [ID={}]\n", id);
        }

        if (!iter->second.texture.isLoaded)
        {
            Logger::Error("Texture not yet loaded! [ID={}]\n", id);
        }

        if (iter->second.referenceCount == 0)
        {
            Logger::Error("Texture reference count is zero! [ID={}]\n", id);
        }

        return iter->second.texture;
    }

    const Vk::Sampler& TextureManager::GetSampler(Vk::SamplerID id) const
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

        if (iter->second.referenceCount == 0)
        {
            Logger::Error("Texture already freed! [ID={}]\n", id);
        }

        --iter->second.referenceCount;

        if (iter->second.referenceCount > 0)
        {
            return;
        }

        deletionQueue.PushDeletor([&megaSet, device, allocator, texture = iter->second.texture] () mutable
        {
            megaSet.FreeSampledImage(texture.descriptorID);
            texture.Destroy(device, allocator);
        });

        m_textureMap.erase(iter);
    }

    void TextureManager::ImGuiDisplay()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Texture Manager"))
            {
                for (const auto& [id, info] : m_textureMap)
                {
                    const auto& [texture, referenceCount] = info;

                    if (!texture.isLoaded)
                    {
                        continue;
                    }

                    if (referenceCount == 0)
                    {
                        continue;
                    }

                    if (ImGui::TreeNode(std::bit_cast<void*>(id), "%s", texture.name.c_str()))
                    {
                        ImGui::Text("ID               | %llu", id);
                        ImGui::Text("Reference Count  | %llu", referenceCount);
                        ImGui::Text("Descriptor Index | %u",   texture.descriptorID);
                        ImGui::Text("Width            | %u",   texture.image.width);
                        ImGui::Text("Height           | %u",   texture.image.height);
                        ImGui::Text("Depth            | %u",   texture.image.depth);
                        ImGui::Text("Mipmap Levels    | %u",   texture.image.mipLevels);
                        ImGui::Text("Array Layers     | %u",   texture.image.arrayLayers);
                        ImGui::Text("Format           | %s",   string_VkFormat(texture.image.format));
                        ImGui::Text("Usage            | %s",   string_VkImageUsageFlags(texture.image.usage).c_str());

                        ImGui::Separator();

                        const f32 originalWidth  = static_cast<f32>(texture.image.width);
                        const f32 originalHeight = static_cast<f32>(texture.image.height);

                        constexpr f32 MAX_SIZE = 512.0f;

                        // Maintain aspect ratio
                        const f32  scale     = std::min(MAX_SIZE / originalWidth, MAX_SIZE / originalHeight);
                        const auto imageSize = ImVec2(originalWidth * scale, originalHeight * scale);

                        ImGui::Image(texture.descriptorID, imageSize);

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
        for (auto& [texture, _] : m_textureMap | std::views::values)
        {
            texture.Destroy(device, allocator);
        }

        for (const auto& sampler : m_samplerMap | std::views::values)
        {
            sampler.Destroy(device);
        }
    }
}
