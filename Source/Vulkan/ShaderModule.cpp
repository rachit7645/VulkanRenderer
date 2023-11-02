/*
 * Copyright 2023 Rachit Khandelwal
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

#include "ShaderModule.h"

#include "Util/Log.h"
#include "Util/Files.h"

// Usings
using Engine::Files;

// Shader directory
constexpr auto ASSETS_SHADERS_DIR = "Shaders/";

namespace Vk
{
    VkShaderModule CreateShaderModule(VkDevice device, const std::string_view path)
    {
        // Get files handle
        auto files = Files::GetInstance();
        // Calc full path
        auto fullPath = fmt::format("{}{}{}", files.GetResources(), ASSETS_SHADERS_DIR, path);

        // Get binary data
        auto shaderBinary = files.ReadBytes(fullPath);

        // Shader module creation info
        VkShaderModuleCreateInfo createInfo =
        {
            .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext    = nullptr,
            .flags    = 0,
            .codeSize = static_cast<u32>(shaderBinary.size()),
            .pCode    = reinterpret_cast<u32*>(shaderBinary.data())
        };

        // Shader module
        VkShaderModule shaderModule = {};

        // Create shader module
        if (vkCreateShaderModule(
                device,
                &createInfo,
                nullptr,
                &shaderModule
            ) != VK_SUCCESS)
        {
            // Log
            LOG_ERROR("Failed to create shader module for shader binary {}!\n", path);
        }

        // Log
        LOG_INFO("Created shader module {} [handle={}]\n", path, reinterpret_cast<void*>(shaderModule));

        // Return
        return shaderModule;
    }
}