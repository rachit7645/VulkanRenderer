/*
 * Copyright (c) 2023 - 2026 Rachit
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

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/utility/vk_format_utils.h>

#include "Util.h"
#include "DebugUtils.h"
#include "Util/Log.h"

namespace Vk
{
    f64 GetTexelSize(VkFormat format)
    {
        const f64        texelBlockSize = vkuFormatTexelBlockSize(format);
        const VkExtent3D blockExtent    = vkuFormatTexelBlockExtent(format);

        const usize texelsPerBlock = blockExtent.width * blockExtent.height * blockExtent.depth;

        return texelBlockSize / static_cast<f64>(std::min(texelsPerBlock, 1ull));
    }

    void CheckResult(VkResult result, const std::string_view message)
    {
        if (result != VK_SUCCESS)
        {
            Logger::VulkanError("[{}] {}\n", string_VkResult(result), message.data());
        }
    }
}