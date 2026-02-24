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

#ifndef IMAGE_UPLOADER_H
#define IMAGE_UPLOADER_H

#include <variant>
#include <vulkan/vulkan.h>
#include <ktx.h>

#include "Image.h"
#include "Buffer.h"
#include "BarrierWriter.h"
#include "Util/DeletionQueue.h"
#include "Vulkan/StagingPool.h"

namespace Vk
{
    enum class ImageUploadType : u8
    {
        SDR  = 0,
        HDR  = 1,
        EXR  = 2,
        KTX2 = 3,
        RAW  = 4
    };

    enum class ImageUploadFlags : u8
    {
        None    = 0,
        Flipped = 1 << 0,
        F16     = 1 << 1,
        Mipmaps = 1 << 2
    };

    struct ImageUploadFile
    {
        std::string path = "Null/File";
    };

    struct ImageUploadMemory
    {
        std::string     name = "Null/Memory";
        std::vector<u8> data = {};
    };

    struct ImageUploadRawMemory
    {
        std::string     name   = "Null/RawMemory";
        u32             width  = 0;
        u32             height = 0;
        VkFormat        format = VK_FORMAT_UNDEFINED;
        std::vector<u8> data   = {};
    };

    struct ImageUpdateRawMemory
    {
        VkRect2D        update = {};
        std::vector<u8> data   = {};
    };

    using ImageUploadSource = std::variant<ImageUploadFile, ImageUploadMemory, ImageUploadRawMemory>;

    struct ImageUpload
    {
        ImageUploadType   type   = ImageUploadType::SDR;
        ImageUploadFlags  flags  = ImageUploadFlags::None;
        ImageUploadSource source = {};
    };

    class ImageUploader
    {
    public:
        [[nodiscard]] Vk::Image LoadImage
        (
            VkDevice device,
            VmaAllocator allocator,
            Vk::StagingPool& stagingPool,
            Util::DeletionQueue& deletionQueue,
            const Vk::ImageUpload& upload
        );

        void UpdateImage
        (
            VkDevice device,
            VmaAllocator allocator,
            Vk::StagingPool& stagingPool,
            Util::DeletionQueue& deletionQueue,
            const Vk::Image& image,
            const Vk::ImageUpdateRawMemory& updateRawMemory
        );

        void FlushUploads(const Vk::CommandBuffer& cmdBuffer);

        [[nodiscard]] bool HasPendingUploads();

        void Clear();
    private:
        struct Upload
        {
            Vk::Image                       image           = {};
            VkBuffer                        buffer          = VK_NULL_HANDLE;
            std::vector<VkBufferImageCopy2> copyRegions     = {};
            VkPipelineStageFlags2           srcStageMask    = VK_PIPELINE_STAGE_2_NONE;
            VkAccessFlags2                  srcAccessMask   = VK_ACCESS_2_NONE;
            VkImageLayout                   oldLayout       = VK_IMAGE_LAYOUT_UNDEFINED;
            bool                            generateMipmaps = false;
        };

        [[nodiscard]] Vk::Image LoadFromFile
        (
            VkDevice device,
            VmaAllocator allocator,
            Vk::StagingPool& stagingPool,
            Util::DeletionQueue& deletionQueue,
            const std::string_view path,
            ImageUploadType type,
            ImageUploadFlags flags
        );

        [[nodiscard]] Vk::Image LoadFromMemory
        (
            VkDevice device,
            VmaAllocator allocator,
            Vk::StagingPool& stagingPool,
            Util::DeletionQueue& deletionQueue,
            const Vk::ImageUploadMemory& memory,
            ImageUploadType type,
            ImageUploadFlags flags
        );

        [[nodiscard]] Vk::Image LoadSTBIFile
        (
            VkDevice device,
            VmaAllocator allocator,
            Vk::StagingPool& stagingPool,
            Util::DeletionQueue& deletionQueue,
            const std::string_view path,
            ImageUploadFlags flags
        );

        [[nodiscard]] Vk::Image LoadSTBIMemory
        (
            VkDevice device,
            VmaAllocator allocator,
            Vk::StagingPool& stagingPool,
            Util::DeletionQueue& deletionQueue,
            const Vk::ImageUploadMemory& memory,
            ImageUploadFlags flags
        );

        [[nodiscard]] Vk::Image LoadSTBIInternal
        (
            VkDevice device,
            VmaAllocator allocator,
            Vk::StagingPool& stagingPool,
            Util::DeletionQueue& deletionQueue,
            const u8* data,
            u32 width,
            u32 height,
            ImageUploadFlags flags
        );

        [[nodiscard]] Vk::Image LoadHDRFile
        (
            VkDevice device,
            VmaAllocator allocator,
            Vk::StagingPool& stagingPool,
            Util::DeletionQueue& deletionQueue,
            const std::string_view path,
            ImageUploadFlags flags
        );

        [[nodiscard]] Vk::Image LoadHDRMemory
        (
            VkDevice device,
            VmaAllocator allocator,
            Vk::StagingPool& stagingPool,
            Util::DeletionQueue& deletionQueue,
            const Vk::ImageUploadMemory& memory,
            ImageUploadFlags flags
        );

        [[nodiscard]] Vk::Image LoadHDRInternal
        (
            VkDevice device,
            VmaAllocator allocator,
            Vk::StagingPool& stagingPool,
            Util::DeletionQueue& deletionQueue,
            const f32* data,
            u32 width,
            u32 height,
            ImageUploadFlags flags
        );

        [[nodiscard]] Vk::Image LoadEXRFile
        (
            VkDevice device,
            VmaAllocator allocator,
            Vk::StagingPool& stagingPool,
            Util::DeletionQueue& deletionQueue,
            const std::string_view path,
            ImageUploadFlags flags
        );

        [[nodiscard]] Vk::Image LoadKTX2File
        (
            VkDevice device,
            VmaAllocator allocator,
            Vk::StagingPool& stagingPool,
            Util::DeletionQueue& deletionQueue,
            const std::string_view path
        );

        [[nodiscard]] Vk::Image LoadKTX2Memory
        (
            VkDevice device,
            VmaAllocator allocator,
            Vk::StagingPool& stagingPool,
            Util::DeletionQueue& deletionQueue,
            const Vk::ImageUploadMemory& memory
        );

        [[nodiscard]] Vk::Image LoadKTX2Internal
        (
            VkDevice device,
            VmaAllocator allocator,
            Vk::StagingPool& stagingPool,
            Util::DeletionQueue& deletionQueue,
            ktxTexture2* pTexture
        );

        [[nodiscard]] Vk::Image LoadRawMemory
        (
            VkDevice device,
            VmaAllocator allocator,
            Vk::StagingPool& stagingPool,
            Util::DeletionQueue& deletionQueue,
            const ImageUploadRawMemory& rawMemory,
            ImageUploadFlags flags
        );

        void AppendUpload(Upload&& upload);

        std::vector<Upload> m_pendingUploads;
        std::mutex          m_mutex;

        Vk::BarrierWriter m_barrierWriter;
    };
}

#endif