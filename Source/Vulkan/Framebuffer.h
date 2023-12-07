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

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <span>
#include <vulkan/vulkan.h>

#include "ImageView.h"
#include "Externals/GLM.h"

namespace Vk
{
    class Framebuffer
    {
    public:
        // Default constructor
        Framebuffer() = default;

        // Create framebuffer
        Framebuffer
        (
            VkDevice device,
            VkRenderPass renderPass,
            const std::span<const Vk::ImageView> attachments,
            const glm::uvec2& size,
            u32 layers
        );

        // Destroy framebuffer
        void Destroy(VkDevice device);

        // Framebuffer handle
        VkFramebuffer handle = VK_NULL_HANDLE;
        // Size
        glm::uvec2 size = {0, 0};
        // Layers
        u32 layers = 0;
    };
}

#endif
