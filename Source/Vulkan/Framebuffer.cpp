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

#include "RenderPass.h"
#include "Framebuffer.h"
#include "Util/Log.h"

#include <vector>

namespace Vk
{
    Framebuffer::Framebuffer
    (
        VkDevice device,
        const Vk::RenderPass& renderPass,
        const std::span<const Vk::ImageView> attachments,
        const glm::uvec2& size,
        u32 layers
    )
        : size(size),
          layers(layers)
    {
        // Attachments in Vulkan's format
        std::vector<VkImageView> vkAttachments = {};
        vkAttachments.reserve(attachments.size());

        // Copy
        std::transform(attachments.begin(), attachments.end(), std::back_inserter(vkAttachments),
        [] (const auto& attachment)
        {
           // Return
           return attachment.handle;
        });

        // Framebuffer info
        VkFramebufferCreateInfo framebufferInfo =
        {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .pNext           = nullptr,
            .flags           = 0,
            .renderPass      = renderPass.handle,
            .attachmentCount = static_cast<u32>(vkAttachments.size()),
            .pAttachments    = vkAttachments.data(),
            .width           = size.x,
            .height          = size.y,
            .layers          = layers
        };

        // Create framebuffer
        if (vkCreateFramebuffer(
                device,
                &framebufferInfo,
                nullptr,
                &handle
            ) != VK_SUCCESS)
        {
            // Log
            Logger::Error
            (
                "Failed to create framebuffer! [device={}] [renderPass={}]\n",
                reinterpret_cast<void*>(device),
                reinterpret_cast<void*>(renderPass.handle)
            );
        }
    }

    void Framebuffer::Destroy(VkDevice device) const
    {
        // Log
        Logger::Debug("Destroying framebuffer! [handle={}]\n", reinterpret_cast<void*>(handle));
        // Destroy
        vkDestroyFramebuffer(device, handle, nullptr);
    }
}