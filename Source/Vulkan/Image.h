/*
 *    Copyright 2023 Rachit Khandelwal
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef VK_IMAGE_H
#define VK_IMAGE_H

#include "Buffer.h"
#include "Context.h"
#include "Util/Util.h"
#include "CommandBuffer.h"

namespace Vk
{
    class Image
    {
    public:
        // Default constructor to make c++ happy
        Image() = default;

        // Image with mipmaps
        Image
        (
            const std::shared_ptr<Vk::Context>& context,
            u32 width,
            u32 height,
            u32 mipLevels,
            VkFormat format,
            VkImageTiling tiling,
            VkImageAspectFlags aspect,
            VkImageUsageFlags usage,
            VkMemoryPropertyFlags properties
        );

        // Copy image from handle
        Image
        (
            VkImage image,
            u32 width,
            u32 height,
            u32 mipLevels,
            VkFormat format,
            VkImageTiling tiling,
            VkImageAspectFlags aspect
        );

        // Comparison operator
        bool operator==(const Image& rhs) const;

        // Transitions image layout
        void TransitionLayout
        (
            const Vk::CommandBuffer& cmdBuffer,
            VkImageLayout oldLayout,
            VkImageLayout newLayout
        );

        // Copies buffer data into image
        void CopyFromBuffer(const std::shared_ptr<Vk::Context>& context, Vk::Buffer& buffer);

        // Delete image
        void Destroy(VmaAllocator allocator) const;

        // Vulkan handles
        VkImage       handle     = VK_NULL_HANDLE;
        VmaAllocation allocation = {};

        // Image dimensions
        u32 width     = 0;
        u32 height    = 0;
        u32 mipLevels = 0;

        // Image properties
        VkFormat           format = VK_FORMAT_UNDEFINED;
        VkImageTiling      tiling = VK_IMAGE_TILING_OPTIMAL;
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    private:
        // Create image & allocate memory for image
        void CreateImage
        (
            VmaAllocator allocator,
            VkImageUsageFlags usage,
            VkMemoryPropertyFlags properties
        );
    };
}

// Don't nuke me for this
namespace std
{
    // Hashing
    template<>
    struct hash<Vk::Image>
    {
        std::size_t operator()(const Vk::Image& image) const
        {
            // Return combined hashes
            return std::hash<VkImage>()(image.handle) ^ std::hash<VmaAllocation>()(image.allocation);
        }
    };
}

#endif
