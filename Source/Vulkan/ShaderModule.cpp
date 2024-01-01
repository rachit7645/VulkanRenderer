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

#include "ShaderModule.h"

#include "Util/Log.h"
#include "Util/Files.h"

namespace Vk
{
    // Shader directory
    constexpr auto ASSETS_SHADERS_DIR = "Shaders/";

    ShaderModule::ShaderModule(VkDevice device, const std::string_view path)
    {
        // Calc full path
        auto fullPath = Engine::Files::GetAssetPath(ASSETS_SHADERS_DIR, path);

        // Get binary data
        auto shaderBinary = Engine::Files::ReadBytes(fullPath);

        // Shader module creation info
        VkShaderModuleCreateInfo createInfo =
        {
            .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext    = nullptr,
            .flags    = 0,
            .codeSize = static_cast<u32>(shaderBinary.size()),
            .pCode    = reinterpret_cast<u32*>(shaderBinary.data())
        };

        // Create shader module
        if (vkCreateShaderModule(
                device,
                &createInfo,
                nullptr,
                &handle
            ) != VK_SUCCESS)
        {
            // Log
            Logger::Error("Failed to create shader module for shader binary {}!\n", path);
        }

        // Log
        Logger::Info("Created shader module {} [handle={}]\n", path, std::bit_cast<void*>(handle));
    }

    void ShaderModule::Destroy(VkDevice device) const
    {
        // Log
        Logger::Debug("Destroying shader module [handle={}]\n", std::bit_cast<void*>(handle));
        // Destroy
        vkDestroyShaderModule(device, handle, nullptr);
    }
}