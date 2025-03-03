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
        usize pathHash = std::hash<std::string_view>()(path);

        for (const auto& [id, textureInfo] : textureMap)
        {
            if (pathHash == textureInfo.pathHash)
            {
                return id;
            }
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
                pathHash,
                Engine::Files::GetNameWithoutExtension(path),
                texture
            )
        );

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
                std::hash<std::string_view>()(name),
                name.data(),
                texture
            )
        );

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
        const auto id = megaSet.WriteSampledImage(texture.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        textureMap.emplace
        (
            id,
            TextureInfo(
                std::hash<std::string_view>{}(name),
                name.data(),
                texture
            )
        );

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
        Vk::BeginLabel(cmdBuffer, "Texture Transfer", {0.6117f, 0.8196f, 0.0313f, 1.0f});

        for (auto& [texture, upload] : m_pendingUploads)
        {
            texture.UploadToGPU(cmdBuffer, upload);
        }

        Vk::EndLabel(cmdBuffer);
    }

    void TextureManager::Clear(VmaAllocator allocator)
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

    void TextureManager::ImGuiDisplay()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Texture Manager"))
            {
                for (const auto& [id, textureInfo] : textureMap)
                {
                    if (ImGui::TreeNode(std::bit_cast<void*>(textureInfo.pathHash), textureInfo.name.c_str()))
                    {
                        ImGui::Text("Descriptor Index | %u", id);
                        ImGui::Text("Width            | %u", textureInfo.texture.image.width);
                        ImGui::Text("Height           | %u", textureInfo.texture.image.height);
                        ImGui::Text("Depth            | %u", textureInfo.texture.image.depth);
                        ImGui::Text("Mipmap Levels    | %u", textureInfo.texture.image.mipLevels);
                        ImGui::Text("Array Layers     | %u", textureInfo.texture.image.arrayLayers);
                        ImGui::Text("Format           | %s", string_VkFormat(textureInfo.texture.image.format));
                        ImGui::Text("Usage            | %s", string_VkImageUsageFlags(textureInfo.texture.image.usage).c_str());

                        ImGui::Separator();

                        const f32 originalWidth  = static_cast<f32>(textureInfo.texture.image.width);
                        const f32 originalHeight = static_cast<f32>(textureInfo.texture.image.height);

                        constexpr f32 MAX_SIZE = 512.0f;

                        // Maintain aspect ratio
                        const f32  scale     = std::min(MAX_SIZE / originalWidth, MAX_SIZE / originalHeight);
                        const auto imageSize = ImVec2(originalWidth * scale, originalHeight * scale);

                        ImGui::Image(id, imageSize);

                        ImGui::TreePop();
                    }

                    ImGui::Separator();
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
    }

    void TextureManager::Destroy(VkDevice device, VmaAllocator allocator)
    {
        for (auto& [id, name, texture] : textureMap | std::views::values)
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