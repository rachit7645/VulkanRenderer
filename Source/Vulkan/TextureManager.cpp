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
    Vk::TextureID TextureManager::LoadTexture
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

        auto loadedIter = m_textures.find(id);

        if (loadedIter != m_textures.end())
        {
            ++loadedIter->second.referenceCount;

            return id;
        }

        auto pendingIter = m_pendingTextures.find(id);

        if (pendingIter != m_pendingTextures.end())
        {
            ++pendingIter->second.referenceCount;

            return id;
        }

        auto future = executor.async([this, device, allocator, &stagingPool, &executor, &deletionQueue, upload] ()
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
        });

        m_pendingTextures.emplace(id, TextureManager::TextureLoadInfo
        {
            .name           = nameInfo.name,
            .future   = std::move(future),
            .referenceCount = 1
        });

        return id;
    }

    Vk::TextureID TextureManager::RegisterTexture
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

        if (m_textures.contains(id))
        {
            return id;
        }

        const auto descriptorID = megaSet.WriteSampledImage(imageView);

        m_textures.emplace(id, TextureInfo{
            .texture = Vk::Texture{
                .name           = std::string(name),
                .image          = image,
                .imageView      = imageView,
                .descriptorID   = descriptorID
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

        auto iter = m_samplers.find(id);

        if (iter != m_samplers.end())
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

        m_samplers.emplace(id, TextureManager::SamplerInfo{
            .sampler        = sampler,
            .referenceCount = 1
        });

        megaSet.Update(device);

        return id;
    }

    void TextureManager::Update
    (
        VkDevice device,
        const Vk::CommandBuffer& cmdBuffer,
        Vk::MegaSet& megaSet,
        Scratch::Allocator& scratchAllocator
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        const std::scoped_lock lock{m_mutex};

        if (!m_imageUploader.HasPendingUploads() && m_pendingTextures.empty())
        {
            return;
        }

        for (auto& [id, info] : m_pendingTextures)
        {
            #ifdef ENGINE_PROFILE
            ZoneNamed(zone, true);
            zone.NameFmt("%s", info.name.c_str());
            #endif

            if (info.referenceCount == 0)
            {
                Logger::Error("Texture reference count is zero! [ID={}]\n", id.value);
            }

            if (!info.future.valid())
            {
                Logger::Error("Future is not valid! [ID={}]\n", id.value);
            }

            info.future.wait();

            const auto [image, imageView] = info.future.get();

            Vk::SetDebugName(device, image.handle,     info.name);
            Vk::SetDebugName(device, imageView.handle, info.name + "_View");

            m_textures.emplace(id, TextureManager::TextureInfo
            {
                .texture        = Vk::Texture{
                    .name         = info.name,
                    .image        = image,
                    .imageView    = imageView,
                    .descriptorID = megaSet.WriteSampledImage(imageView)
                },
                .referenceCount = info.referenceCount
            });
        }

        m_pendingTextures.clear();

        Vk::BeginLabel(cmdBuffer, "TextureManager::Update", {0.6117f, 0.1196f, 0.0313f, 1.0f});

        m_imageUploader.FlushUploads(cmdBuffer, scratchAllocator);

        megaSet.Update(device);

        Vk::EndLabel(cmdBuffer);
    }

    void TextureManager::ForceUpdate
    (
        Vk::TextureID id,
        VkDevice device,
        const Vk::CommandBuffer& cmdBuffer,
        Vk::MegaSet& megaSet,
        Scratch::Allocator& scratchAllocator
    )
    {
        #ifdef ENGINE_PROFILE
        ZoneScoped;
        #endif

        const std::scoped_lock lock{m_mutex};

        if (IsLoadedInternal(id))
        {
            return;
        }

        auto pendingIter = m_pendingTextures.find(id);

        if (pendingIter == m_pendingTextures.end())
        {
            Logger::Error("Invalid texture ID! [ID={}]\n", id.value);
        }

        auto& textureLoadInfo = pendingIter->second;

        if (textureLoadInfo.referenceCount == 0)
        {
            Logger::Error("Texture reference count is zero! [ID={}]\n", id.value);
        }

        if (!textureLoadInfo.future.valid())
        {
            Logger::Error("Future is not valid! [ID={}]\n", id.value);
        }

        textureLoadInfo.future.wait();

        const auto [image, imageView] = textureLoadInfo.future.get();

        Vk::SetDebugName(device, image.handle,     textureLoadInfo.name);
        Vk::SetDebugName(device, imageView.handle, textureLoadInfo.name + "_View");

        m_textures.emplace(id, TextureManager::TextureInfo
        {
            .texture        = Vk::Texture{
                .name         = textureLoadInfo.name,
                .image        = image,
                .imageView    = imageView,
                .descriptorID = megaSet.WriteSampledImage(imageView)
            },
            .referenceCount = textureLoadInfo.referenceCount
        });

        m_pendingTextures.erase(pendingIter);

        Vk::BeginLabel(cmdBuffer, "TextureManager::ForceUpdate", {0.6117f, 0.1196f, 0.0313f, 1.0f});

        m_imageUploader.FlushUploads(cmdBuffer, scratchAllocator);

        megaSet.Update(device);

        Vk::EndLabel(cmdBuffer);
    }

    void TextureManager::UpdateTexture
    (
        Vk::TextureID id,
        VkDevice device,
        VmaAllocator allocator,
        Vk::StagingPool& stagingPool,
        Util::DeletionQueue& deletionQueue,
        const Vk::ImageUpdateRawMemory& updateRawMemory
    )
    {
        const std::scoped_lock lock{m_mutex};

        const auto& texture = GetTextureInternal(id);

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

    bool TextureManager::IsLoadedInternal(Vk::TextureID id)
    {
        auto iter = m_textures.find(id);

        if (iter == m_textures.end())
        {
            return false;
        }

        if (iter->second.referenceCount == 0)
        {
            Logger::Error("Texture reference count is zero! [ID={}]\n", id.value);
        }

        return true;
    }

    bool TextureManager::IsLoaded(Vk::TextureID id)
    {
        const std::scoped_lock lock{m_mutex};

        return IsLoadedInternal(id);
    }

    Vk::Texture& TextureManager::GetTextureInternal(Vk::TextureID id)
    {
        auto iter = m_textures.find(id);

        if (iter == m_textures.end())
        {
            Logger::Error("Invalid texture id! [ID={}]\n", id.value);
        }

        if (iter->second.referenceCount == 0)
        {
            Logger::Error("Texture reference count is zero! [ID={}]\n", id.value);
        }

        return iter->second.texture;
    }

    const Vk::Texture& TextureManager::GetTexture(Vk::TextureID id)
    {
        const std::scoped_lock lock{m_mutex};

        return GetTextureInternal(id);
    }

    const Vk::Sampler& TextureManager::GetSampler(Vk::SamplerID id) const
    {
        const auto iter = m_samplers.find(id);

        if (iter == m_samplers.cend())
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
        Vk::TextureID id,
        VkDevice device,
        VmaAllocator allocator,
        Vk::MegaSet& megaSet,
        Util::DeletionQueue& deletionQueue
    )
    {
        const std::scoped_lock lock{m_mutex};

        const auto loadedIter = m_textures.find(id);

        if (loadedIter != m_textures.end())
        {
            if (loadedIter->second.referenceCount == 0)
            {
                Logger::Error("Texture already freed! [ID={}]\n", id.value);
            }

            --loadedIter->second.referenceCount;

            if (loadedIter->second.referenceCount > 0)
            {
                return;
            }

            deletionQueue.Push([&megaSet, device, allocator, texture = loadedIter->second.texture] () mutable
            {
                megaSet.FreeSampledImage(texture.descriptorID);
                texture.Destroy(device, allocator);
            });

            m_textures.erase(loadedIter);
        }

        const auto pendingIter = m_pendingTextures.find(id);

        if (pendingIter != m_pendingTextures.end())
        {
            if (pendingIter->second.referenceCount == 0)
            {
                Logger::Error("Texture already freed! [ID={}]\n", id.value);
            }

            --pendingIter->second.referenceCount;

            if (pendingIter->second.referenceCount > 0)
            {
                return;
            }

            deletionQueue.Push([device, allocator, future = std::move(pendingIter->second.future)] () mutable
            {
                if (!future.valid())
                {
                    return;
                }

                future.wait();

                const auto [image, imageView] = future.get();

                imageView.Destroy(device);
                image.Destroy(allocator);
            });

            m_pendingTextures.erase(pendingIter);
        }
    }

    void TextureManager::DestroySampler
    (
        Vk::SamplerID id,
        VkDevice device,
        Vk::MegaSet& megaSet,
        Util::DeletionQueue& deletionQueue
    )
    {
        const auto iter = m_samplers.find(id);

        if (iter == m_samplers.end())
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

        m_samplers.erase(id);
    }

    void TextureManager::ImGuiDisplay()
    {
        if (ImGui::CollapsingHeader("Textures"))
        {
            const std::scoped_lock lock{m_mutex};

            VkDeviceSize totalMemoryUsed = 0;

            for (const auto& [id, info] : m_textures)
            {
                const auto& [texture, referenceCount] = info;

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
            for (const auto& [id, info] : m_samplers)
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

        return m_imageUploader.HasPendingUploads() || !m_pendingTextures.empty();
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
        for (auto& [texture, _] : m_textures | std::views::values)
        {
            texture.Destroy(device, allocator);
        }

        for (const auto& [sampler, _] : m_samplers | std::views::values)
        {
            sampler.Destroy(device);
        }
    }
}
