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

#ifndef VK_IMAGE_H
#define VK_IMAGE_H

#include "Barrier.h"
#include "CommandBuffer.h"
#include "Util/Types.h"
#include "Externals/VMA.h"

namespace Vk
{
    class Image
    {
    public:
        Image() = default;

        Image(VmaAllocator allocator, const VkImageCreateInfo& createInfo, VkImageAspectFlags aspect);

        Image
        (
            VkImage image,
            u32 width,
            u32 height,
            u32 depth,
            u32 mipLevels,
            u32 arrayLayers,
            VkFormat format,
            VkImageAspectFlags aspect
        );

        bool operator==(const Image& rhs) const;

        void Barrier(const Vk::CommandBuffer& cmdBuffer, const Vk::ImageBarrier& barrier) const;

        void GenerateMipmaps(const Vk::CommandBuffer& cmdBuffer) const;

        void Destroy(VmaAllocator allocator) const;

        VkImage       handle     = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;

        u32 width       = 0;
        u32 height      = 0;
        u32 depth       = 0;
        u32 mipLevels   = 0;
        u32 arrayLayers = 0;

        VkFormat           format = VK_FORMAT_UNDEFINED;
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_NONE;

        VkDeviceSize size = 0;
    };
}

#endif
