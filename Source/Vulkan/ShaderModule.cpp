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