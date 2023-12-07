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

#ifndef SUBPASS_STATE_H
#define SUBPASS_STATE_H

#include <vector>
#include <optional>
#include <vulkan/vulkan.h>

namespace Vk
{
    // Subpass
    struct SubpassState
    {
        // Subpass description
        VkSubpassDescription description;
        // Color attachment references
        std::vector<VkAttachmentReference> colorReferences = {};
        // Depth attachment reference
        std::optional<VkAttachmentReference> depthReference;
        // Subpass dependency
        VkSubpassDependency dependency = {};
    };
}

#endif
