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

#include "SubpassBuilder.h"
#include "RenderPassBuilder.h"
#include "Util/Optional.h"

namespace Vk::Builders
{
    SubpassBuilder SubpassBuilder::Create()
    {
        // Return
        return {};
    }

    SubpassState SubpassBuilder::Build() const
    {
        // Return subpass state
        return subpassState;
    }

    SubpassBuilder& SubpassBuilder::AddColorReference(u32 attachment, VkImageLayout layout)
    {
        // Attachment reference
        VkAttachmentReference reference =
        {
            .attachment = attachment,
            .layout     = layout
        };

        // Add
        subpassState.colorReferences.emplace_back(reference);

        // Return
        return *this;
    }

    SubpassBuilder& SubpassBuilder::AddDepthReference(u32 attachment, VkImageLayout layout)
    {
        // Set depth reference
        subpassState.depthReference =
        {
            .attachment = attachment,
            .layout     = layout
        };

        // Return
        return *this;
    }

    SubpassBuilder& SubpassBuilder::SetBindPoint(VkPipelineBindPoint bindPoint)
    {
        // Set
        subpassState.description =
        {
            .flags                   = 0,
            .pipelineBindPoint       = bindPoint,
            .inputAttachmentCount    = 0,
            .pInputAttachments       = nullptr,
            .colorAttachmentCount    = static_cast<u32>(subpassState.colorReferences.size()),
            .pColorAttachments       = subpassState.colorReferences.data(),
            .pResolveAttachments     = nullptr,
            .pDepthStencilAttachment = Util::GetAddressOrNull(subpassState.depthReference),
            .preserveAttachmentCount = 0,
            .pPreserveAttachments    = nullptr
        };

        // Return
        return *this;
    }

    SubpassBuilder& SubpassBuilder::AddDependency
    (
        uint32_t srcSubpass,
        uint32_t dstSubpass,
        VkPipelineStageFlags srcStageMask,
        VkPipelineStageFlags dstStageMask,
        VkAccessFlags srcAccessMask,
        VkAccessFlags dstAccessMask
    )
    {
        // Dependency info
        VkSubpassDependency dependency =
        {
            .srcSubpass      = srcSubpass,
            .dstSubpass      = dstSubpass,
            .srcStageMask    = srcStageMask,
            .dstStageMask    = dstStageMask,
            .srcAccessMask   = srcAccessMask,
            .dstAccessMask   = dstAccessMask,
            .dependencyFlags = 0
        };

        // Add
        subpassState.dependencies.emplace_back(dependency);

        // Return
        return *this;
    }
}