#ifndef SHADER_MODULE_H
#define SHADER_MODULE_H

#include <string_view>
#include <vector>
#include <vulkan/vulkan.h>

#include "Util/Util.h"

namespace Vk
{
    // Creates a shader module
    [[nodiscard]] VkShaderModule CreateShaderModule(VkDevice device, const std::string_view path);
}

#endif
