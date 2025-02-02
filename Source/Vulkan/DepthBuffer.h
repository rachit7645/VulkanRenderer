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

#ifndef DEPTH_BUFFER_H
#define DEPTH_BUFFER_H

#include <memory>
#include <vulkan/vulkan.h>

#include "Image.h"
#include "ImageView.h"
#include "Context.h"
#include "FormatHelper.h"

namespace Vk
{
    class DepthBuffer
    {
    public:
        DepthBuffer() = default;

        DepthBuffer(const Vk::Context& context, const Vk::FormatHelper& formatHelper, VkExtent2D extent);
        void Destroy(VkDevice device, VmaAllocator allocator) const;

        Vk::Image     depthImage     = {};
        Vk::ImageView depthImageView = {};
    };
}

#endif
