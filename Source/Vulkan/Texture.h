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
        // Constructor
        Texture(const std::shared_ptr<Vk::Context>& context, const std::string_view path);
        // Equality operator
        bool operator==(const Texture& rhs) const;
        // Destroy texture
        void Destroy(VkDevice device) const;

        // Texture image data
        Vk::Image image = {};
        // Texture image view
        Vk::ImageView imageView = {};

        // Hashing operator
        struct Hash
        {
            usize operator()(const Texture& texture) const
            {
                // Just return the hash of the handle
                return Vk::Image::Hash()(texture.image) ^ Vk::ImageView::Hash()(texture.imageView);
            }
        };

        // Equality operator
        struct Equal
        {
            bool operator()(const Texture& lhs, const Texture& rhs) const
            {
                // Compare
                return lhs == rhs;
            }
        };
    };
}

#endif
