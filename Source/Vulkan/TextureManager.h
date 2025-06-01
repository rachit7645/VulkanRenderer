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
#include "Util/Types.h"
#include "Externals/Taskflow.h"

namespace Vk
{
    using TextureID = u64;
    using SamplerID = u64;

    class TextureManager
    {
    public:
        [[nodiscard]] Vk::TextureID AddTexture
        (
            VmaAllocator allocator,
            Util::DeletionQueue& deletionQueue,
            const Vk::ImageUpload& upload
        );

        [[nodiscard]] Vk::TextureID AddTexture
        (
            Vk::MegaSet& megaSet,
            VkDevice device,
            const std::string_view name,
            const Vk::Image& image,
            const Vk::ImageView& imageView
        );

        [[nodiscard]] Vk::SamplerID AddSampler
        (
            Vk::MegaSet& megaSet,
            VkDevice device,
            const VkSamplerCreateInfo& createInfo
        );

        void Update(const Vk::CommandBuffer& cmdBuffer, VkDevice device, Vk::MegaSet& megaSet);

        [[nodiscard]] const Vk::Texture& GetTexture(Vk::TextureID id) const;
        [[nodiscard]] const Vk::Sampler& GetSampler(Vk::SamplerID id) const;

        void DestroyTexture
        (
            Vk::TextureID id,
            VkDevice device,
            VmaAllocator allocator,
            Vk::MegaSet& megaSet,
            Util::DeletionQueue& deletionQueue
        );

        void ImGuiDisplay();

        [[nodiscard]] bool HasPendingUploads();

        void Destroy(VkDevice device, VmaAllocator allocator);
    private:
        struct TextureInfo
        {
            Vk::Texture texture        = {};
            u64         referenceCount = 0;
        };

        std::unordered_map<Vk::TextureID, TextureInfo> m_textureMap;
        std::unordered_map<Vk::SamplerID, Vk::Sampler> m_samplerMap;

        Vk::ImageUploader m_imageUploader;

        tf::Executor                                              m_executor;
        std::unordered_map<Vk::TextureID, std::future<Vk::Image>> m_futuresMap;
    };
}

#endif
