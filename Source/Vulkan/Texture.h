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

#ifndef TEXTURE_H
#define TEXTURE_H

#include <vulkan/vulkan.h>

#include "Image.h"
#include "ImageView.h"
#include "Context.h"
#include "Util/Util.h"
#include "Externals/STBImage.h"

namespace Vk
{
    class Texture
    {
    public:
        // Texture flags
        enum class Flags : u8
        {
            // Flags
            None       = 0,
            IsSRGB     = 1U << 0,
            GenMipmaps = 1U << 1
        };

        // Constructor
        Texture(const std::shared_ptr<Vk::Context>& context, const std::string_view path, Flags flags = Flags::None);
        // Equality operator
        bool operator==(const Texture& rhs) const;
        // Destroy texture
        void Destroy(VkDevice device, VmaAllocator allocator) const;

        // Texture image data
        Vk::Image image = {};
        // Texture image view
        Vk::ImageView imageView = {};
    private:
        // Generate mipmaps
        void GenerateMipmaps(const std::shared_ptr<Vk::Context>& context);
    };
}

// Don't nuke me for this
namespace std
{
    // Hashing
    template <>
    struct hash<Vk::Texture>
    {
        std::size_t operator()(const Vk::Texture& texture) const
        {
            // Combine hashes
            return hash<Vk::Image>()(texture.image) ^ hash<Vk::ImageView>()(texture.imageView);
        }
    };
}

#endif
