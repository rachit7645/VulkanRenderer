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

#include "STBImage.h"
#include "Util/Log.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace STB
{
    Image::Image(const std::string_view path, s32 requiredComponents)
    {
        s32 _width  = 0;
        s32 _height = 0;

        data = stbi_load(path.data(), &_width, &_height, &channels, requiredComponents);

        if (data == nullptr)
        {
            Logger::Error("Failed to load image file \"{}\"", path.data());
        }

        width  = static_cast<u32>(_width);
        height = static_cast<u32>(_height);
    }

    Image::~Image()
    {
        stbi_image_free(data);
    }
}