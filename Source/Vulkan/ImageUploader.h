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

#ifndef IMAGE_UPLOADER_H
#define IMAGE_UPLOADER_H

#include <vulkan/vulkan.h>

#include "Image.h"
#include "Buffer.h"
#include "BarrierWriter.h"
#include "Util/DeletionQueue.h"

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
        std::vector<u8> data = {};
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
            VmaAllocator allocator,
            Util::DeletionQueue& deletionQueue,
            const Vk::ImageUpload& upload
        );

        void FlushUploads(const Vk::CommandBuffer& cmdBuffer);

        [[nodiscard]] bool HasPendingUploads();

        void Clear();
    private:
        struct Upload
        {
            Vk::Image                       image;
            Vk::Buffer                      buffer;
            std::vector<VkBufferImageCopy2> copyRegions;
        };

        [[nodiscard]] Vk::Image LoadFromFile
        (
            VmaAllocator allocator,
            Util::DeletionQueue& deletionQueue,
            const std::string_view path,
            ImageUploadType type,
            ImageUploadFlags flags
        );

        [[nodiscard]] Vk::Image LoadFromMemory
        (
            VmaAllocator allocator,
            Util::DeletionQueue& deletionQueue,
            const Vk::ImageUploadMemory& memory,
            ImageUploadType type,
            ImageUploadFlags flags
        );

        [[nodiscard]] Vk::Image LoadSTBIFile
        (
            VmaAllocator allocator,
            Util::DeletionQueue& deletionQueue,
            const std::string_view path,
            ImageUploadFlags flags
        );

        [[nodiscard]] Vk::Image LoadSTBIMemory
        (
            VmaAllocator allocator,
            Util::DeletionQueue& deletionQueue,
            const Vk::ImageUploadMemory& memory,
            ImageUploadFlags flags
        );

        [[nodiscard]] Vk::Image LoadSTBIInternal
        (
            VmaAllocator allocator,
            Util::DeletionQueue& deletionQueue,
            const u8* data,
            u32 width,
            u32 height
        );

        [[nodiscard]] Vk::Image LoadHDRFile
        (
            VmaAllocator allocator,
            Util::DeletionQueue& deletionQueue,
            const std::string_view path,
            ImageUploadFlags flags
        );

        [[nodiscard]] Vk::Image LoadHDRMemory
        (
            VmaAllocator allocator,
            Util::DeletionQueue& deletionQueue,
            const Vk::ImageUploadMemory& memory,
            ImageUploadFlags flags
        );

        [[nodiscard]] Vk::Image LoadHDRInternal
        (
            VmaAllocator allocator,
            Util::DeletionQueue& deletionQueue,
            const f32* data,
            u32 width,
            u32 height,
            ImageUploadFlags flags
        );

        [[nodiscard]] Vk::Image LoadEXRFile
        (
            VmaAllocator allocator,
            Util::DeletionQueue& deletionQueue,
            const std::string_view path,
            ImageUploadFlags flags
        );

        [[nodiscard]] Vk::Image LoadKTX2File
        (
            VmaAllocator allocator,
            Util::DeletionQueue& deletionQueue,
            const std::string_view path
        );

        [[nodiscard]] Vk::Image LoadKTX2Memory
        (
            VmaAllocator allocator,
            Util::DeletionQueue& deletionQueue,
            const Vk::ImageUploadMemory& memory
        );

        [[nodiscard]] Vk::Image LoadKTX2Internal
        (
            VmaAllocator allocator,
            Util::DeletionQueue& deletionQueue,
            ktxTexture2* pTexture
        );

        [[nodiscard]] Vk::Image LoadRawMemory
        (
            VmaAllocator allocator,
            Util::DeletionQueue& deletionQueue,
            const ImageUploadRawMemory& rawMemory
        );

        void AppendUpload(Upload&& upload);

        std::vector<Upload> m_pendingUploads;
        std::mutex          m_uploadMutex;

        Vk::BarrierWriter m_barrierWriter;
    };
}

#endif