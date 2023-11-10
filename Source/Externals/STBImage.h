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

#ifndef EXT_STBI_IMAGE_H
#define EXT_STBI_IMAGE_H

#include "Util/Util.h"

namespace STB
{
    // Loaded image from STB
    struct STBImage
    {
        // Load image
        explicit STBImage(const std::string_view path, s32 requiredComponents);
        // Free image
        ~STBImage();
        // Pixel data (PLEASE DON'T BE INCOMPATIBLE SOME DAY 60 YEARS LATER)
        u8* data = nullptr;
        // Metadata
        u32 width    = 0;
        u32 height   = 0;
        s32 channels = 0;
    };
}

#endif
