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

#ifndef TEXTURE_MANAGER_H
#define TEXTURE_MANAGER_H

#include "Texture.h"
#include "ImageUploader.h"
#include "Sampler.h"
#include "MegaSet.h"
#include "Util/Util.h"

namespace Vk
{
    class TextureManager
    {
    public:
        struct TextureInfo
        {
            std::string name;
            Vk::Texture texture;
        };

        [[nodiscard]] u32 AddTexture
        (
            VkDevice device,
            VmaAllocator allocator,
            Vk::MegaSet& megaSet,
            Util::DeletionQueue& deletionQueue,
            const std::string_view path
        );

        [[nodiscard]] u32 AddTexture
        (
            VkDevice device,
            VmaAllocator allocator,
            Vk::MegaSet& megaSet,
            Util::DeletionQueue& deletionQueue,
            const std::string_view name,
            VkFormat format,
            const void* data,
            u32 width,
            u32 height
        );

        [[nodiscard]] u32 AddTexture
        (
            Vk::MegaSet& megaSet,
            VkDevice device,
            const std::string_view name,
            const Vk::Texture& texture
        );

        [[nodiscard]] u32 AddSampler
        (
            Vk::MegaSet& megaSet,
            VkDevice device,
            const VkSamplerCreateInfo& createInfo
        );

        void Update(const Vk::CommandBuffer& cmdBuffer);

        [[nodiscard]] const Vk::Texture& GetTexture(u32 id) const;
        [[nodiscard]] const Vk::Sampler& GetSampler(u32 id) const;

        void DestroyTexture
        (
            u32 id,
            VkDevice device,
            VmaAllocator allocator,
            Vk::MegaSet& megaSet,
            Util::DeletionQueue& deletionQueue
        );

        void ImGuiDisplay();

        [[nodiscard]] bool HasPendingUploads() const;

        void Destroy(VkDevice device, VmaAllocator allocator);

        std::unordered_map<u32, TextureInfo> textureMap;
        std::unordered_map<u32, Vk::Sampler> samplerMap;
    private:
        Vk::ImageUploader m_imageUploader;

        std::unordered_map<usize, u32> m_nameHashToTextureIDMap;
    };
}

#endif
