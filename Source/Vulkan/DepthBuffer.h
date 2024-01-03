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

#ifndef DEPTH_BUFFER_H
#define DEPTH_BUFFER_H

#include <memory>
#include <vulkan/vulkan.h>

#include "Image.h"
#include "ImageView.h"
#include "Context.h"

namespace Vk
{
    class DepthBuffer
    {
    public:
        // Default constructor
        DepthBuffer() = default;
        // Create depth buffer
        DepthBuffer(const std::shared_ptr<Vk::Context>& context, VkExtent2D swapchainExtent);
        // Destroy depth buffer
        void Destroy(VkDevice device, VmaAllocator allocator) const;

        // Get depth format
        static VkFormat GetDepthFormat(VkPhysicalDevice physicalDevice);

        // Depth image
        Vk::Image depthImage = {};
        // Depth image view
        Vk::ImageView depthImageView = {};
    };
}

#endif
