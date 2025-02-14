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

#include "ShaderModule.h"

#include <vector>
#include <volk/volk.h>

#include "DebugUtils.h"
#include "Util/Log.h"
#include "Util/Files.h"
#include "Util/Util.h"
#include "Util.h"

namespace Vk
{
    // Shader directory
    constexpr auto ASSETS_SHADERS_DIR = "Shaders/";

    ShaderModule::ShaderModule(VkDevice device, const std::string_view path)
    {
        const auto fullPath     = Engine::Files::GetAssetPath(ASSETS_SHADERS_DIR, path) + ".spv";
        const auto shaderBinary = Engine::Files::ReadBytes(fullPath);

        const VkShaderModuleCreateInfo createInfo =
        {
            .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext    = nullptr,
            .flags    = 0,
            .codeSize = static_cast<u32>(shaderBinary.size()),
            .pCode    = reinterpret_cast<const u32*>(shaderBinary.data())
        };

        Vk::CheckResult(vkCreateShaderModule(
            device,
            &createInfo,
            nullptr,
            &handle),
            fmt::format("Failed to create shader module! [Path={}]!", path)
        );

        Vk::SetDebugName(device, handle, Engine::Files::GetNameWithoutExtension(path));

        Logger::Debug("Created shader module {} [handle={}]\n", path, std::bit_cast<void*>(handle));
    }

    void ShaderModule::Destroy(VkDevice device) const
    {
        Logger::Debug("Destroying shader module [handle={}]\n", std::bit_cast<void*>(handle));
        vkDestroyShaderModule(device, handle, nullptr);
    }
}