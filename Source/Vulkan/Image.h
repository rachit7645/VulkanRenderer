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

#ifndef VK_IMAGE_H
#define VK_IMAGE_H

#include "Barrier.h"
#include "CommandBuffer.h"
#include "Util/Util.h"

namespace Vk
{
    class Image
    {
    public:
        // Default constructor to make C++ happy
        Image() = default;

        Image(VmaAllocator allocator, const VkImageCreateInfo& createInfo, VkImageAspectFlags aspect);

        // Basic constructor for copying
        Image
        (
            VkImage image,
            u32 width,
            u32 height,
            u32 depth,
            u32 mipLevels,
            u32 arrayLayers,
            VkFormat format,
            VkImageUsageFlags usage,
            VkImageAspectFlags aspect
        );

        bool operator==(const Image& rhs) const;

        void Barrier(const Vk::CommandBuffer& cmdBuffer, const Vk::ImageBarrier& barrier) const;

        void GenerateMipmaps(const Vk::CommandBuffer& cmdBuffer) const;

        void Destroy(VmaAllocator allocator) const;

        // Vulkan handles
        VkImage           handle         = VK_NULL_HANDLE;
        VmaAllocation     allocation     = VK_NULL_HANDLE;
        VmaAllocationInfo allocationInfo = {};

        // Image dimensions
        u32 width       = 0;
        u32 height      = 0;
        u32 depth       = 0;
        u32 mipLevels   = 0;
        u32 arrayLayers = 0;

        // Image properties
        VkFormat           format = VK_FORMAT_UNDEFINED;
        VkImageUsageFlags  usage  = 0;
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_NONE;
    };
}

// Hashing
template<>
struct std::hash<Vk::Image>
{
    std::size_t operator()(const Vk::Image& image) const noexcept
    {
        const std::size_t hash1 = std::hash<VkImage      >{}(image.handle);
        const std::size_t hash2 = std::hash<VmaAllocation>{}(image.allocation);
        const std::size_t hash3 = std::hash<u32          >{}(image.width);
        const std::size_t hash4 = std::hash<u32          >{}(image.height);
        const std::size_t hash5 = std::hash<VkFormat     >{}(image.format);
        const std::size_t hash6 = std::hash<u32          >{}(image.mipLevels);
        const std::size_t hash7 = std::hash<u32          >{}(image.arrayLayers);

        // Mix hashes using std::rotl for better distribution
        return hash1 ^
               std::rotl(hash2, 1) ^
               std::rotl(hash3, 2) ^
               std::rotl(hash4, 3) ^
               std::rotl(hash5, 4) ^
               std::rotl(hash6, 5) ^
               std::rotl(hash7, 6);
    }
};

#endif
