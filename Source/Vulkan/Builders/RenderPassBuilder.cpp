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

#include "RenderPassBuilder.h"
#include "Util/Log.h"
#include "Util/Util.h"

namespace Vk
{
    RenderPassBuilder RenderPassBuilder::Create(VkDevice device)
    {
        // Return
        return RenderPassBuilder(device);
    }

    RenderPassBuilder::RenderPassBuilder(VkDevice device)
        : m_device(device)
    {
    }

    VkRenderPass RenderPassBuilder::Build()
    {
        // Subpass descriptions
        std::vector<VkSubpassDescription> subpasses = {};
        subpasses.reserve(subpassStates.size());
        // Subpass dependencies
        std::vector<VkSubpassDependency> dependencies = {};
        subpasses.reserve(subpassStates.size());

        // Copy subpass descriptions
        std::transform(subpassStates.begin(), subpassStates.end(), std::back_inserter(subpasses),
        [](const auto& subpassState)
        {
            // Return
            return subpassState.description;
        });

        // Copy subpass dependencies
        std::transform(subpassStates.begin(), subpassStates.end(), std::back_inserter(dependencies),
        [](const auto& subpassState)
        {
            // Return
            return subpassState.dependency;
        });

        // Render pass creation info
        VkRenderPassCreateInfo createInfo =
        {
            .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .pNext           = nullptr,
            .flags           = 0,
            .attachmentCount = static_cast<u32>(descriptions.size()),
            .pAttachments    = descriptions.data(),
            .subpassCount    = static_cast<u32>(subpasses.size()),
            .pSubpasses      = subpasses.data(),
            .dependencyCount = static_cast<u32>(dependencies.size()),
            .pDependencies   = dependencies.data()
        };

        // Create render pass
        VkRenderPass renderPass = VK_NULL_HANDLE;
        if (vkCreateRenderPass(
            m_device,
            &createInfo,
            nullptr,
            &renderPass
            ) != VK_SUCCESS)
        {
            // Log
            Logger::Error("Failed to create render pass! [device={}]\n", reinterpret_cast<void*>(m_device));
        }

        // Log
        Logger::Info("Created render pass! [handle={}]\n", reinterpret_cast<void*>(renderPass));

        // Return
        return renderPass;
    }

    RenderPassBuilder& RenderPassBuilder::AddSubpass(const Vk::SubpassState& subpass)
    {
        // Add
        subpassStates.emplace_back(subpass);
        // Return
        return *this;
    }

    RenderPassBuilder& RenderPassBuilder::AddAttachment
    (
        VkFormat format,
        VkSampleCountFlagBits samples,
        VkAttachmentLoadOp loadOp,
        VkAttachmentStoreOp storeOp,
        VkAttachmentLoadOp stencilLoadOp,
        VkAttachmentStoreOp stencilStoreOp,
        VkImageLayout initialLayout,
        VkImageLayout finalLayout
    )
    {
        // Attachment description
        VkAttachmentDescription description =
        {
            .flags          = 0,
            .format         = format,
            .samples        = samples,
            .loadOp         = loadOp,
            .storeOp        = storeOp,
            .stencilLoadOp  = stencilLoadOp,
            .stencilStoreOp = stencilStoreOp,
            .initialLayout  = initialLayout,
            .finalLayout    = finalLayout
        };

        // Add
        descriptions.emplace_back(description);

        // Return
        return *this;
    }
}