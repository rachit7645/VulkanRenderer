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

#ifndef TEXTURE_MANAGER_H
#define TEXTURE_MANAGER_H

#include "Texture.h"
#include "ImageUploader.h"
#include "Sampler.h"
#include "MegaSet.h"
#include "SamplerID.h"
#include "TextureID.h"
#include "Util/Types.h"
#include "Externals/Taskflow.h"
#include "Externals/UnorderedDense.h"

namespace Vk
{
    class TextureManager
    {
    public:
        [[nodiscard]] Vk::TextureID LoadTexture
        (
            VkDevice device,
            VmaAllocator allocator,
            Vk::StagingPool& stagingPool,
            tf::Executor& executor,
            Util::DeletionQueue& deletionQueue,
            const Vk::ImageUpload& upload
        );

        [[nodiscard]] Vk::TextureID RegisterTexture
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

        // WARNING! Blocks this thread!
        void Update
        (
            VkDevice device,
            const Vk::CommandBuffer& cmdBuffer,
            Vk::MegaSet& megaSet,
            Scratch::Allocator& scratchAllocator
        );

        // WARNING! Blocks this thread!
        // This should be faster than calling Update
        // But only if you only need this specific texture to be ready
        void ForceUpdate
        (
            Vk::TextureID id,
            VkDevice device,
            const Vk::CommandBuffer& cmdBuffer,
            Vk::MegaSet& megaSet,
            Scratch::Allocator& scratchAllocator
        );

        void UpdateTexture
        (
            Vk::TextureID id,
            VkDevice device,
            VmaAllocator allocator,
            Vk::StagingPool& stagingPool,
            Util::DeletionQueue& deletionQueue,
            const Vk::ImageUpdateRawMemory& updateRawMemory
        );

        [[nodiscard]] bool IsLoaded(Vk::TextureID id);

        [[nodiscard]] const Vk::Texture& GetTexture(Vk::TextureID id);
        [[nodiscard]] const Vk::Sampler& GetSampler(Vk::SamplerID id) const;

        void DestroyTexture
        (
            Vk::TextureID id,
            VkDevice device,
            VmaAllocator allocator,
            Vk::MegaSet& megaSet,
            Util::DeletionQueue& deletionQueue
        );

        void DestroySampler
        (
            Vk::SamplerID id,
            VkDevice device,
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

        struct TextureLoadInfo
        {
            std::string                    name;
            std::future<Vk::UploadedImage> future;
            u64                            referenceCount = 0;
        };

        struct SamplerInfo
        {
            Vk::Sampler sampler        = {};
            u64         referenceCount = 0;
        };

        struct TextureNameInfo
        {
            std::string name;
            std::string id;
        };

        [[nodiscard]] bool IsLoadedInternal(Vk::TextureID id);

        [[nodiscard]] Vk::Texture& GetTextureInternal(Vk::TextureID id);

        TextureNameInfo GetTextureNameInfo(const ImageUploadSource& source);

        ankerl::unordered_dense::map<Vk::TextureID, TextureInfo> m_textures;
        ankerl::unordered_dense::map<Vk::SamplerID, SamplerInfo> m_samplers;

        ankerl::unordered_dense::map<Vk::TextureID, TextureLoadInfo> m_pendingTextures;

        Vk::ImageUploader m_imageUploader;

        std::mutex m_mutex;
    };
}
#endif