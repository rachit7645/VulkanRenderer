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

#include "TextureManager.h"

#include <ranges>
#include <vulkan/vk_enum_string_helper.h>

#include "Texture.h"
#include "Util.h"
#include "DebugUtils.h"
#include "Util/Log.h"
#include "Util/Types.h"
#include "Util/Visitor.h"
#include "Util/Files.h"
#include "Externals/ImGui.h"

namespace Vk
{
    Vk::TextureID TextureManager::AddTexture
    (
        VkDevice device,
        VmaAllocator allocator,
        Vk::StagingPool& stagingPool,
        tf::Executor& executor,
        Util::DeletionQueue& deletionQueue,
        const Vk::ImageUpload& upload
    )
    {
        const std::scoped_lock lock{m_mutex};

        const auto nameInfo = GetTextureNameInfo(upload.source);

        const Vk::TextureID id = {.value = std::hash<std::string_view>()(nameInfo.id)};

        auto iter = m_textureMap.find(id);

        if (iter != m_textureMap.end())
        {
            ++iter->second.referenceCount;

            return id;
        }

        m_futuresMap.emplace(id, executor.async([this, device, allocator, &stagingPool, &executor, &deletionQueue, upload] ()
        {
            return m_imageUploader.LoadImage
            (
                device,
                allocator,
                stagingPool,
                executor,
                deletionQueue,
                upload
            );
        }));

        m_textureMap.emplace(id, TextureInfo{
            .texture = Vk::Texture{
                .name           = nameInfo.name,
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
        const std::scoped_lock lock{m_mutex};

        const Vk::TextureID id = {.value = std::hash<std::string_view>()(name)};

        if (m_textureMap.contains(id))
        {
            return id;
        }

        const auto descriptorID = megaSet.WriteSampledImage(imageView);

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

        megaSet.Update(device);

        return id;
    }

    Vk::SamplerID TextureManager::AddSampler
    (
        Vk::MegaSet& megaSet,
        VkDevice device,
        const VkSamplerCreateInfo& createInfo
    )
    {
        const Vk::SamplerID id = {.value = std::hash<VkSamplerCreateInfo>()(createInfo)};

        auto iter = m_samplerMap.find(id);

        if (iter != m_samplerMap.end())
        {
            ++iter->second.referenceCount;

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

        Vk::SetDebugName(device, sampler.handle, fmt::format("Sampler/{}", id.value));

        m_samplerMap.emplace(id, TextureManager::SamplerInfo{
            .sampler        = sampler,
            .referenceCount = 1
        });

        megaSet.Update(device);

        return id;
    }

    void TextureManager::Update(const Vk::CommandBuffer& cmdBuffer, VkDevice device, Vk::MegaSet& megaSet)
    {
        const std::scoped_lock lock{m_mutex};

        if (!m_imageUploader.HasPendingUploads() && m_futuresMap.empty())
        {
            return;
        }

        for (auto& [id, info] : m_textureMap)
        {
            auto& [texture, referenceCount] = info;

            if (texture.isLoaded)
            {
                continue;
            }

            if (referenceCount == 0)
            {
                continue;
            }

            auto iter = m_futuresMap.find(id);

            if (iter == m_futuresMap.end())
            {
                continue;
            }

            auto& future = iter->second;

            if (!future.valid())
            {
                Logger::Error("Future is not valid! [ID={}]", id.value);
            }

            future.wait();

            const auto upload = future.get();

            texture.image     = upload.image;
            texture.imageView = upload.imageView;

            texture.descriptorID = megaSet.WriteSampledImage(texture.imageView);

            texture.isLoaded = true;

            Vk::SetDebugName(device, texture.image.handle,     texture.name);
            Vk::SetDebugName(device, texture.imageView.handle, texture.name + "_View");
        }

        m_futuresMap.clear();

        Vk::BeginLabel(cmdBuffer, "TextureManager::Update", {0.6117f, 0.1196f, 0.0313f, 1.0f});

        m_imageUploader.FlushUploads(cmdBuffer);

        megaSet.Update(device);

        Vk::EndLabel(cmdBuffer);
    }

    void TextureManager::UpdateTexture
    (
        const Vk::TextureID id,
        VkDevice device,
        VmaAllocator allocator,
        Vk::StagingPool& stagingPool,
        Util::DeletionQueue& deletionQueue,
        const Vk::ImageUpdateRawMemory& updateRawMemory
    )
    {
        const auto& texture = GetTexture(id);

        m_imageUploader.UpdateImage
        (
            device,
            allocator,
            stagingPool,
            deletionQueue,
            texture.image,
            updateRawMemory
        );
    }

    Vk::Texture& TextureManager::GetTexture(const Vk::TextureID id)
    {
        auto iter = m_textureMap.find(id);

        if (iter == m_textureMap.end())
        {
            Logger::Error("Invalid texture id! [ID={}]\n", id.value);
        }

        if (!iter->second.texture.isLoaded)
        {
            Logger::Error("Texture not yet loaded! [ID={}]\n", id.value);
        }

        if (iter->second.referenceCount == 0)
        {
            Logger::Error("Texture reference count is zero! [ID={}]\n", id.value);
        }

        return iter->second.texture;
    }

    Vk::Sampler& TextureManager::GetSampler(const Vk::SamplerID id)
    {
        auto iter = m_samplerMap.find(id);

        if (iter == m_samplerMap.end())
        {
            Logger::Error("Invalid sampler ID! [ID={}]\n", id.value);
        }

        if (iter->second.referenceCount == 0)
        {
            Logger::Error("Sampler reference count is zero! [ID={}]\n", id.value);
        }

        return iter->second.sampler;
    }

    const Vk::Texture& TextureManager::GetTexture(const Vk::TextureID id) const
    {
        const auto iter = m_textureMap.find(id);

        if (iter == m_textureMap.cend())
        {
            Logger::Error("Invalid texture id! [ID={}]\n", id.value);
        }

        if (!iter->second.texture.isLoaded)
        {
            Logger::Error("Texture not yet loaded! [ID={}]\n", id.value);
        }

        if (iter->second.referenceCount == 0)
        {
            Logger::Error("Texture reference count is zero! [ID={}]\n", id.value);
        }

        return iter->second.texture;
    }

    const Vk::Sampler& TextureManager::GetSampler(const Vk::SamplerID id) const
    {
        const auto iter = m_samplerMap.find(id);

        if (iter == m_samplerMap.cend())
        {
            Logger::Error("Invalid sampler ID! [ID={}]\n", id.value);
        }

        if (iter->second.referenceCount == 0)
        {
            Logger::Error("Sampler reference count is zero! [ID={}]\n", id.value);
        }

        return iter->second.sampler;
    }

    void TextureManager::DestroyTexture
    (
        const Vk::TextureID id,
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
            Logger::Error("Texture already freed! [ID={}]\n", id.value);
        }

        --iter->second.referenceCount;

        if (iter->second.referenceCount > 0)
        {
            return;
        }

        deletionQueue.Push([&megaSet, device, allocator, texture = iter->second.texture] () mutable
        {
            megaSet.FreeSampledImage(texture.descriptorID);
            texture.Destroy(device, allocator);
        });

        m_textureMap.erase(iter);
    }

    void TextureManager::DestroySampler
    (
        const Vk::SamplerID id,
        VkDevice device,
        Vk::MegaSet& megaSet,
        Util::DeletionQueue& deletionQueue
    )
    {
        const auto iter = m_samplerMap.find(id);

        if (iter == m_samplerMap.end())
        {
            return;
        }

        if (iter->second.referenceCount == 0)
        {
            Logger::Error("Sampler already freed! [ID={}]\n", id.value);
        }

        --iter->second.referenceCount;

        if (iter->second.referenceCount > 0)
        {
            return;
        }

        deletionQueue.Push([&megaSet, device, sampler = iter->second.sampler] () mutable
        {
            megaSet.FreeSampler(sampler.descriptorID);
            sampler.Destroy(device);
        });

        m_samplerMap.erase(id);
    }

    void TextureManager::ImGuiDisplay()
    {
        if (ImGui::CollapsingHeader("Textures"))
        {
            VkDeviceSize totalMemoryUsed = 0;

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

                totalMemoryUsed += texture.image.size;

                if (ImGui::TreeNode(std::bit_cast<void*>(id.value), "%s", texture.name.c_str()))
                {
                    ImGui::Text("ID                | %llu",       id.value);
                    ImGui::Text("Reference Count   | %llu",       referenceCount);
                    ImGui::Text("Descriptor Index  | %u",         texture.descriptorID);
                    ImGui::Text("Image Handle      | %p",         static_cast<void*>(texture.image.handle));
                    ImGui::Text("Image View Handle | %p",         static_cast<void*>(texture.imageView.handle));
                    ImGui::Text("Width             | %u",         texture.image.width);
                    ImGui::Text("Height            | %u",         texture.image.height);
                    ImGui::Text("Depth             | %u",         texture.image.depth);
                    ImGui::Text("Mipmap Levels     | %u",         texture.image.mipLevels);
                    ImGui::Text("Array Layers      | %u",         texture.image.arrayLayers);
                    ImGui::Text("Format            | %s",         string_VkFormat(texture.image.format));
                    ImGui::Text("Memory Used       | %llu Bytes", texture.image.size);

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

            ImGui::Text("Total Texture Memory Usage | %llu Bytes", totalMemoryUsed);
        }

        if (ImGui::CollapsingHeader("Samplers"))
        {
            for (const auto& [id, info] : m_samplerMap)
            {
                const auto& [sampler, referenceCount] = info;

                if (referenceCount == 0)
                {
                    continue;
                }

                if (ImGui::TreeNode(std::bit_cast<void*>(id.value), "%llu", id.value))
                {
                    ImGui::Text("Reference Count  | %llu", referenceCount);
                    ImGui::Text("Descriptor Index | %u",   sampler.descriptorID);
                    ImGui::Text("Sampler Handle   | %p",   static_cast<void*>(sampler.handle));

                    ImGui::TreePop();
                }

                ImGui::Separator();
            }
        }
    }

    bool TextureManager::HasPendingUploads()
    {
        const std::scoped_lock lock{m_mutex};

        return m_imageUploader.HasPendingUploads() || !m_futuresMap.empty();
    }

    TextureManager::TextureNameInfo TextureManager::GetTextureNameInfo(const ImageUploadSource& source)
    {
        return std::visit(Util::Visitor{
            [] (const Vk::ImageUploadFile& file)
            {
                return TextureManager::TextureNameInfo
                {
                    .name = Files::GetNameWithoutExtension(file.path),
                    .id   = file.path
                };
            },
            [] (const Vk::ImageUploadMemory& memory)
            {
                return TextureManager::TextureNameInfo
                {
                    .name = memory.name,
                    .id   = memory.name
                };
            },
            [] (const Vk::ImageUploadRawMemory& rawMemory)
            {
                return TextureManager::TextureNameInfo
                {
                    .name = rawMemory.name,
                    .id   = rawMemory.name
                };
            },
            [] (const Vk::ImageUploadCache& cache)
            {
                return TextureManager::TextureNameInfo
                {
                    .name = cache.name,
                    .id   = cache.cachedPath
                };
            }
        }, source);
    }

    void TextureManager::Destroy(VkDevice device, VmaAllocator allocator)
    {
        for (auto& [texture, _] : m_textureMap | std::views::values)
        {
            texture.Destroy(device, allocator);
        }

        for (const auto& [sampler, _] : m_samplerMap | std::views::values)
        {
            sampler.Destroy(device);
        }
    }
}
