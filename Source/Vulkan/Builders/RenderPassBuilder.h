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

#ifndef RENDERPASS_BUILDER_H
#define RENDERPASS_BUILDER_H

#include <vector>
#include <vulkan/vulkan.h>

#include "SubpassState.h"
#include "Vulkan/RenderPass.h"

namespace Vk::Builders
{
    class RenderPassBuilder
    {
    public:
        // Create builder
        [[nodiscard]] static RenderPassBuilder Create(VkDevice device);
        // Build renderpass
        [[nodiscard]] Vk::RenderPass Build();

        // Add attachment
        [[nodiscard]] RenderPassBuilder& AddAttachment
        (
            VkFormat format,
            VkSampleCountFlagBits samples,
            VkAttachmentLoadOp loadOp,
            VkAttachmentStoreOp storeOp,
            VkAttachmentLoadOp stencilLoadOp,
            VkAttachmentStoreOp stencilStoreOp,
            VkImageLayout initialLayout,
            VkImageLayout finalLayout
        );
        // Add subpass
        [[nodiscard]] RenderPassBuilder& AddSubpass(const Builders::SubpassState& subpass);

        // Attachment descriptions
        std::vector<VkAttachmentDescription> descriptions = {};
        // Subpasses
        std::vector<SubpassState> subpassStates = {};
    private:
        // Constructor
        explicit RenderPassBuilder(VkDevice device);
        // Internal data
        VkDevice m_device = VK_NULL_HANDLE;
    };
}

#endif
