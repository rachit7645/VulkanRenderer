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

#ifndef SUBPASS_BUILDER_H
#define SUBPASS_BUILDER_H

#include <vulkan/vulkan.h>

#include "SubpassState.h"
#include "Util/Util.h"

namespace Vk::Builders
{
    class SubpassBuilder
    {
    public:
        // Create builder
        [[nodiscard]] static SubpassBuilder Create();
        // Build renderpass
        [[nodiscard]] SubpassState Build() const;

        // Add color reference to attachment
        [[nodiscard]] SubpassBuilder& AddColorReference(u32 attachment, VkImageLayout layout);
        // Add depth reference to attachment
        [[nodiscard]] SubpassBuilder& AddDepthReference(u32 attachment, VkImageLayout layout);
        // Set description
        [[nodiscard]] SubpassBuilder& SetBindPoint(VkPipelineBindPoint bindPoint);
        // Set dependency
        [[nodiscard]] SubpassBuilder& AddDependency
        (
            uint32_t srcSubpass,
            uint32_t dstSubpass,
            VkPipelineStageFlags srcStageMask,
            VkPipelineStageFlags dstStageMask,
            VkAccessFlags srcAccessMask,
            VkAccessFlags dstAccessMask
        );

        // State
        SubpassState subpassState;
    private:
        // Private constructor
        SubpassBuilder() = default;
    };
}

#endif
