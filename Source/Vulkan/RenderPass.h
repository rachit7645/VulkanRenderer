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

#ifndef RENDERPASS_H
#define RENDERPASS_H

#include <vulkan/vulkan.h>

#include "CommandBuffer.h"
#include "Framebuffer.h"
#include "Util/Util.h"

namespace Vk
{
    class RenderPass
    {
    public:
        // Default constructor
        RenderPass() = default;
        // Create renderpass from handle
        explicit RenderPass(VkRenderPass renderPass);
        // Destroy render pass
        void Destroy(VkDevice device) const;

        // Reset clear state
        void ResetClearValues();
        // Set clear value (color)
        void SetClearValue(const glm::vec4& color);
        // Set clear value (depth & stencil)
        void SetClearValue(f32 depth, u32 stencil);

        // Begin render pass
        void BeginRenderPass
        (
            const Vk::CommandBuffer& cmdBuffer,
            const Framebuffer& framebuffer,
            VkRect2D renderArea,
            VkSubpassContents subpassContents
        );
        // End render pass
        void EndRenderPass(const Vk::CommandBuffer& cmdBuffer);

        // Handle
        VkRenderPass handle = VK_NULL_HANDLE;
        // Clear values
        std::vector<VkClearValue> clearValues = {};
    };
}

#endif
