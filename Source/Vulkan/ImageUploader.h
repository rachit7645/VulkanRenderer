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

namespace Vk
{
    class ImageUploader
    {
    public:
        struct Upload
        {
            Vk::Image                       image;
            Vk::Buffer                      buffer;
            std::vector<VkBufferImageCopy2> copyRegions;
        };

        Vk::Image LoadImageFromFile(VmaAllocator allocator, const std::string_view path);

        Vk::Image LoadImageFromMemory
        (
            VmaAllocator allocator,
            VkFormat format,
            const void* data,
            u32 width,
            u32 height
        );

        void FlushUploads(const Vk::CommandBuffer& cmdBuffer);
        void ClearUploads(VmaAllocator allocator);

        bool HasPendingUploads() const;
    private:
        std::vector<Upload> m_pendingUploads;
    };
}

#endif