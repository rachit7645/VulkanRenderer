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
#include "DescriptorWriter.h"
#include "Sampler.h"
#include "MegaSet.h"
#include "FormatHelper.h"
#include "Util/Util.h"

namespace Vk
{
    class TextureManager
    {
    public:
        struct TextureInfo
        {
            u32         textureID = 0;
            Vk::Texture texture;
        };

        explicit TextureManager(const Vk::FormatHelper& formatHelper);

        [[nodiscard]] usize AddTexture
        (
            Vk::MegaSet& megaSet,
            VkDevice device,
            VmaAllocator allocator,
            const std::string_view path,
            Texture::Flags flags
        );

        [[nodiscard]] usize AddTexture
        (
            Vk::MegaSet& megaSet,
            VkDevice device,
            VmaAllocator allocator,
            const std::string_view name,
            const std::span<const u8> data,
            const glm::uvec2 size,
            Texture::Flags flags
        );

        [[nodiscard]] u32 AddSampler
        (
            Vk::MegaSet& megaSet,
            VkDevice device,
            const VkSamplerCreateInfo& createInfo
        );

        void Update(const Vk::CommandBuffer& cmdBuffer);
        void Clear(VmaAllocator allocator);

        [[nodiscard]] u32 GetTextureID(usize pathHash) const;
        [[nodiscard]] const Vk::Texture& GetTexture(usize pathHash) const;

        [[nodiscard]] const Vk::Sampler& GetSampler(u32 id) const;

        void Destroy(VkDevice device, VmaAllocator allocator);

        std::unordered_map<usize, TextureInfo> textureMap;
        std::unordered_map<u32, Vk::Sampler>   samplerMap;
    private:
        VkFormat m_format     = VK_FORMAT_UNDEFINED;
        VkFormat m_formatSRGB = VK_FORMAT_UNDEFINED;

        std::vector<std::pair<Vk::Texture, Vk::Buffer>> m_pendingUploads;
    };
}

#endif
