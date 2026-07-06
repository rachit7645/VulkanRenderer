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

#ifndef IMAGE_DOWNLOADER_H
#define IMAGE_DOWNLOADER_H

#include "Vulkan/Buffer.h"
#include "Vulkan/GraphicsTimeline.h"
#include "Vulkan/Image.h"

namespace Vk
{
    enum class PostDownloadAction : u8
    {
        Cache
    };

    struct PostDownloadCache
    {
        std::string cacheFile = "Null/Cache/Download";
        u64         hash      = 0;
    };

    using PostDownloadActionData = std::variant<PostDownloadCache>;

    struct ImageDownload
    {
        Vk::Image                  image                  = {};
        Vk::PostDownloadAction     postDownloadAction     = PostDownloadAction::Cache;
        Vk::PostDownloadActionData postDownloadActionData = {};
    };

    class ImageDownloader
    {
    public:
        void RequestDownload(const Vk::ImageDownload& download);

        void Update
        (
            usize frameIndex,
            VkDevice device,
            VmaAllocator allocator,
            const Vk::CommandBuffer& cmdBuffer,
            const Vk::GraphicsTimeline& timeline,
            tf::Executor& executor
        );

        void Destroy(VmaAllocator allocator);
    private:
        struct PendingDownload
        {
            Vk::Image                  image                  = {};
            Vk::PostDownloadAction     postDownloadAction     = PostDownloadAction::Cache;
            Vk::PostDownloadActionData postDownloadActionData = {};
            Vk::Buffer                 readbackBuffer         = {};
            usize                      readbackFrameIndex     = 0;
        };

        std::vector<Vk::ImageDownload>                requestedDownloads = {};
        std::vector<ImageDownloader::PendingDownload> pendingDownloads   = {};
    };
}

#endif
