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

#ifndef TEXTURE_H
#define TEXTURE_H

#include <vulkan/vulkan.h>

#include "Image.h"
#include "ImageView.h"

namespace Vk
{
    class Texture
    {
    public:
        void Destroy(VkDevice device, VmaAllocator allocator);

        bool operator==(const Texture& rhs) const;

        Vk::Image     image;
        Vk::ImageView imageView;
    };
}

// Don't nuke me for this
template <>
struct std::hash<Vk::Texture>
{
    std::size_t operator()(const Vk::Texture& texture) const noexcept
    {
        // Again not the best way but ehh it's fine don't worry about it
        return hash<Vk::Image>()(texture.image) ^ hash<Vk::ImageView>()(texture.imageView);
    }
};

#endif
