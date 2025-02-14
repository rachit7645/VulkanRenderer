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

#include "DebugUtils.h"

namespace Vk
{
    void BeginLabel(UNUSED const Vk::CommandBuffer& cmdBuffer, UNUSED const std::string_view name, UNUSED const glm::vec4& color)
    {
        #ifdef ENGINE_DEBUG
        const VkDebugUtilsLabelEXT label =
        {
            .sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
            .pNext      = nullptr,
            .pLabelName = name.data(),
            .color      = {color.r, color.g, color.b, color.a}
        };

        vkCmdBeginDebugUtilsLabelEXT(cmdBuffer.handle, &label);
        #endif
    }

    void BeginLabel(UNUSED VkQueue queue, UNUSED const std::string_view name, UNUSED const glm::vec4& color)
    {
        #ifdef ENGINE_DEBUG
        const VkDebugUtilsLabelEXT label =
        {
            .sType      = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
            .pNext      = nullptr,
            .pLabelName = name.data(),
            .color      = {color.r, color.g, color.b, color.a}
        };

        vkQueueBeginDebugUtilsLabelEXT(queue, &label);
        #endif
    }

    void EndLabel(UNUSED const Vk::CommandBuffer& cmdBuffer)
    {
        #ifdef ENGINE_DEBUG
        vkCmdEndDebugUtilsLabelEXT(cmdBuffer.handle);
        #endif
    }

    void EndLabel(UNUSED VkQueue queue)
    {
        #ifdef ENGINE_DEBUG
        vkQueueEndDebugUtilsLabelEXT(queue);
        #endif
    }
}